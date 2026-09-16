#include "common.h"

#include "actors/actor_800200.h"

extern void func_8010ABD4();

extern s32 func_80103DD4(VECTOR3*, VECTOR3*);

extern s32 func_8010BC70(GsCOORDINATE2*);

extern void func_8010BE5C(GpActorWork*, VECTOR3*);

extern GpActorFuncTable4 D_actor_800200_80161EB8;

extern GpActorFuncTable12 D_actor_800200_80161E5C;

extern TaskFuncTable11 D_actor_800200_80161E8C;

extern GpActorFuncTable3 D_actor_800200_80161E34;

extern GpActorFuncTable9 D_actor_800200_80161EC8;

extern GpActorPathStep D_actor_800200_8016A018[];

extern GpActorPathStep D_actor_800200_8016A108[];

extern GpActorPathStep D_actor_800200_8016A128[];

extern GpActorPathStep D_actor_800200_8016A040[];

extern GpActorPathStep D_actor_800200_8016A090[];

extern GpActorPathStep D_actor_800200_8016A0B0[];

extern GpActorPathStep D_actor_800200_8016A0E0[];

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162750);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162990);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162BFC);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162E0C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163044);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163180);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_8016337C);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163584);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_801637B4);

void func_actor_800200_8016390C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0B0[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0B0[2].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A0B0[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0B0[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 2) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80163A54);

void func_actor_800200_80163B90(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0E0[4].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0E0[4].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            return;
        case 1:
            actor->field_20 = D_actor_800200_8016A0E0[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0E0[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 4) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

void func_actor_800200_80163CCC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A108[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A108[3].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A108[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A108[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 3) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

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

void func_actor_800200_801652EC(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable3 sp;

    sp    = D_actor_800200_80161E34;
    actor = arg0->actor;
    if ((s8)actor->field_97A > 0) {
        actor->field_97A--;
    }
    sp.funcs[actor->field_954](arg0);
    func_actor_800200_80165104(arg0);
    actor->field_986 = 0;
}

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

void func_actor_800200_8016545C(GpActorWork* arg0, s8 arg1)
{
    GameActor* actor = arg0->actor;
    GameActor* actor2;
    u16        flag;

    actor->field_910->field_CC = arg1;
    if (Gp_StateF0.field_0 == 1) {
        actor->field_90C = Gp_FindLockNode(arg0);
    } else {
        actor->field_90C = 0;
    }
    flag              = actor->field_90C != 0;
    actor2            = arg0->actor;
    actor2->field_954 = 0;
    actor2->field_956 = 4;
    actor2->field_958 = 0;
    actor2->field_95A = 0;
    actor2->field_95C = 0;
    actor2->field_95E = 0;
    actor2->field_940 = flag;
}

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
            func_actor_800200_8016390C(arg0);
            return;
        case 15:
            func_actor_800200_80163E14();
            return;
        case 19:
            func_actor_800200_80163CCC(arg0);
            return;
        case 20:
            func_actor_800200_80163B90(arg0);
            return;
        case 24:
            func_actor_800200_80165ACC();
            return;
    }
}

void func_actor_800200_80165814(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    s32            arg;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A018[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A018[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        arg = 6;
        if (d4->field_CE == 2) {
            arg = 5;
        }
        func_actor_800200_80165408(arg0, arg);
    }
}

void func_actor_800200_801658E0(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A040[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A040[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

void func_actor_800200_8016599C(GpActorWork* arg0)
{
    arg0->actor->field_910->field_D0 = 1;
    func_actor_800200_801654EC(arg0, 0);
}

void func_actor_800200_801659CC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u32            state;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_20 = D_actor_800200_8016A090[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A090[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                actor->field_960++;
                if (d4->field_D0 != 1) {
                    func_actor_800200_80165534(arg0);
                }
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            d4->field_D0 = state;
            func_actor_800200_801654EC(arg0, 0);
            break;
    }
}

void func_actor_800200_80165ACC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A128[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A128[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_80165534(arg0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

void func_actor_800200_80165B84(GpActorWork* arg0)
{
    GameActor*         actor;
    GpActorD4*         d4;
    GsCOORDINATE2*     coord;
    GpActorFuncTable12 sp;
    s32                pan;

    sp    = D_actor_800200_80161E5C;
    actor = arg0->actor;
    d4    = actor->field_910;
    coord = arg0->extra->field_8;
    if (d4->field_C4 > 0) {
        d4->field_C4--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x4072000A, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800200_80165CB4(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_800200_80161E8C;
    sp.funcs[arg0->spawnArg1 & 0xF](arg0);
}

void func_actor_800200_80165D44(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;

    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    actor  = arg0->actor;
    switch (actor->field_95E) {
        case 1:
            actor->field_95C  = 0;
            actor->field_95E += 1;
            Gp_AnimResetChildSlots(arg0, 9);
        case 2:
            if ((func_8010BC70(coord) >= 0x500) || (Gp_StateF0.field_0 == 1)) {
                actor->field_95C  = 7;
                actor->field_95E += 1;
                Gp_AnimPlayChildSlotsEx(arg0, 8, 0, 3);
            }
            break;
        case 4:
            Gp_ResetActorMove(arg0, 0);
            break;
        default:
        case 0:
        case 3:
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

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

void func_actor_800200_80165F50(GpActorWork* arg0)
{
    GpActorFuncTable9 sp;
    GameActor*        actor;
    GsCOORDINATE2*    coord;

    sp    = D_actor_800200_80161EC8;
    actor = arg0->actor;
    coord = arg0->extra->field_8;
    sp.funcs[actor->field_956](arg0);
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80165FF0);

s32 func_actor_800200_801660E8(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2)
{
    s32 dist;

    if (arg1->field_4 != 0) {
        dist = func_80103D8C(arg0->workm.t[0] - arg1->field_8, arg0->workm.t[2] - arg1->field_C);
        if (arg2 != NULL) {
            arg2->field_0 = arg1->field_8;
            arg2->field_2 = arg1->field_A;
            arg2->field_0 = arg1->field_C;
        }
    } else {
        dist = 0;
    }
    return dist;
}
