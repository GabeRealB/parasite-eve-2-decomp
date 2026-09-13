#include "common.h"

#include "actors/actor_800200.h"

extern void func_8010ABD4();

extern GpActorFuncTable4 D_actor_800200_80161EB8;

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162750);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162990);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162BFC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162E0C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163044);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163180);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_8016337C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163584);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801637B4);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_8016390C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163A54);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163B90);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163CCC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163E14);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163F5C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80164180);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_8016436C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80164598);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801647A8);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801649D8);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80164C54);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80164EBC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165104);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801652EC);

void func_actor_800200_80165380(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 1;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

void func_actor_800200_801653A0(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 2;
    actor->field_95A = 2;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

void func_actor_800200_801653C0(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 7;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
}

void func_actor_800200_80165408(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_956 = 8;
    actor->field_954 = 0;
    actor->field_958 = 5;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_934 = arg1;
}

void func_actor_800200_80165434(GpActorWork* arg0, s16 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 4;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_940 = arg1;
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_8016545C);

void func_actor_800200_801654EC(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 9;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
}

void func_actor_800200_80165534(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_956 = 0xB;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 0xE, 0, 3);
}

void func_actor_800200_80165580(GpActorWork* arg0)
{
    u8 temp_v1;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = Game_Session->field_6;
    switch (temp_v1) {
        case 26:
            func_actor_800200_80162990();
            return;
        case 24:
            func_actor_800200_80165814();
            return;
        case 23:
            func_actor_800200_80162BFC();
            return;
        case 25:
            func_actor_800200_801658E0();
            return;
    }
}

void func_actor_800200_80165644(GpActorWork* arg0)
{
    u8 temp_v1;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = Game_Session->field_6;
    switch (temp_v1) {
        case 25:
            func_actor_800200_8016599C();
            return;
        case 23:
            func_actor_800200_80163044();
            return;
        case 22:
            func_actor_800200_80163180();
            return;
        case 20:
            func_actor_800200_8016337C();
            return;
    }
}

void func_actor_800200_80165708(GpActorWork* arg0)
{
    u8 temp_v0;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v0 = Game_Session->field_6;
    switch (temp_v0) {
        case 1:
            func_actor_800200_80163A54();
            return;
        case 2:
            func_actor_800200_801637B4();
            return;
        case 3:
            func_actor_800200_801659CC();
            return;
        case 4:
            func_actor_800200_80163584();
            return;
        case 5:
            func_actor_800200_8016390C();
            return;
        case 15:
            func_actor_800200_80163E14();
            return;
        case 19:
            func_actor_800200_80163CCC();
            return;
        case 20:
            func_actor_800200_80163B90();
            return;
        case 24:
            func_actor_800200_80165ACC();
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165814);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801658E0);

void func_actor_800200_8016599C(GpActorWork* arg0)
{
    arg0->actor->field_910->field_D0 = 1;
    func_actor_800200_801654EC(arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801659CC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165ACC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165B84);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165CB4);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165D44);

void func_actor_800200_80165E50(GpActorWork* arg0)
{
    u16 state = arg0->actor->field_95E;

    if (state != 0) {
        if (state == 1) {
            Gp_ResetActorMove(arg0, 0);
        }
    }
}

void func_actor_800200_80165E90(GpActorWork* arg0)
{
    GpActorFuncTable4 sp;
    GameActor*        actor;

    sp    = D_actor_800200_80161EB8;
    actor = arg0->actor;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    sp.funcs[(u16)actor->field_96C](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800200_80165F28(void)
{
    func_8010ABD4();
}

void func_actor_800200_80165F48(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165F50);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165FF0);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801660E8);
