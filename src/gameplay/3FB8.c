#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/wipsys.h"

extern WipSysConfig D_80073B88;
extern TaskDesc     D_80113340[];

s32   func_800B9D80(s32 arg0);
s32   func_8005414C(s32 arg0, s32 arg1, s32 arg2);
void  func_800EC9C8(void);
void  func_800ECA10(s32 arg0);
void  func_800ECA54(void);
s32   func_80037164(void);
Task* func_8002CFA0(TaskDesc* table, s32 idx, s32 arg2, s32 arg3);
void  func_80103A18(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_80103AC0(GpActorWork* arg0);
void  func_801041FC(GpActorWork* arg0, s32 arg1);
void  func_80104B54(void);
void  func_80104E00(void);
void  func_80105070(void);
void  func_801053A0(void);
s32   func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void  func_80105B0C(GpActorWork* arg0);
void  func_80105ED4(GpActorWork* arg0);
void  func_8010615C(GpActorWork* arg0);
void  func_801065A8(GpActorWork* arg0);
void  func_801066DC(GpActorWork* arg0, s32 arg1);
void  func_80108684(GpActorWork* arg0);
void  func_8010870C(GpActorWork* arg0, s32 arg1);
void  func_80108770(GpActorWork* arg0, s32 arg1);
void  func_80109290(GpActorWork* arg0);
void  func_80109374(GpActorWork* arg0);
void  func_801093DC(GpActorWork* arg0);
void  func_80109844(GpActorWork* arg0);
void  func_80109A1C(GpActorWork* arg0);
void  func_8010AAB4(GpActorWork* arg0);
void  func_8010ABD4(GpActorWork* arg0);
void  func_8010AC54(GpActorWork* arg0);
void  func_8010AD64(GpActorWork* arg0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F75BC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F77F8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F7AD4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F7E28);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F8244);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F8A38);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F91AC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F9474);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F96B0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800F9FBC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FA45C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FA7CC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FAA14);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FAC40);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FB148);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FB67C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FB7E4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FBAB0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FBEBC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC0B4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC500);

void func_800FC6C0(void)
{
    D_80115740->field_1A |= 0x80;
}

void func_800FC6E0(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC6F4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC74C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FC9BC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FCD00);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FD49C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FDB18);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FE034);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FE41C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FE56C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FEAF8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FEFA4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FF710);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_800FFA8C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100020);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801005D8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100784);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100B78);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100E40);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80100FCC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801011D0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010133C);

void func_801013FC(Task* arg0)
{
    arg0->state = 3;
}

void func_80101408(GpActorWork* arg0)
{
    volatile GameActor* inner;
    Task*               task;

    inner          = arg0->actor;
    arg0->field_18 = NULL;
    D_80115760     = NULL;
    task           = inner->field_914;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_918;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_91C;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_920;
    if (task != NULL) {
        Task_Kill(task);
    }
    task = inner->field_924;
    if (task != NULL) {
        Task_Kill(task);
    }
    func_800E1638((GpObj*)inner->field_AC);
    func_800E1638((GpObj*)inner->field_CC);
    func_800E1638((GpObj*)inner->field_EC);
    func_800E1638((GpObj*)inner->field_10C);
    func_800E1638((GpObj*)inner->field_12C);
    Task_Kill((Task*)arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801014E8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010154C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80101848);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80101A68);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80101F58);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102348);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102634);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801029D4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102D20);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80102F10);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801030CC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103294);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801034C0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801036FC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103804);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103874);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801038F8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010397C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103A18);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103AC0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103B1C);

void func_80103B5C(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != NULL) {
        node->field_5    = 0;
        inner->field_90C = NULL;
    }
    inner->field_97E = 1;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103B88);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103C74);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103CB4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103D8C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103DD4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103E7C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103F70);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010403C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801040A0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801041B4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801041FC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104258);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104364);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801043F4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104490);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104508);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104684);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104838);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104A4C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104AAC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104B54);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104CAC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104D68);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104E00);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104F5C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105070);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105190);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801052B8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801053A0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801054D8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801055D4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105690);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105754);

s32 func_80105828(GpActorWork* arg0)
{
    return arg0->actor->field_982;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010583C);

s32 func_80105894(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;

    actor = (GameActor*)((arg1 * sizeof(GameActorSlot)) + (s32)arg0->actor);
    return (actor->field_448[0].field_0 & 0x102) == 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801058BC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105914);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801059AC);

s32 func_80105A60(Task* arg0, s32 arg1, s32 arg2)
{
    func_800B57EC((GsCOORDINATE2*)arg2, (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8);
    return 0;
}

s32 func_80105A8C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_958 = 1;
    } else {
        inner->field_958 = 3;
    }
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105AB0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105B0C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105B74);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105BC4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105ED4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801060E0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010615C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801061F0);

void func_80106238(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_124 = (actor->field_124 & 0xFFFF3FFF) | (((arg1 << 1) | arg2) << 14);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106264);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801062DC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106350);

void func_801064A4(GpObj38* arg0, s32 arg1, s32 arg2)
{
    s32 temp;

    temp = (s8)func_800D937C(arg0);
    func_8005414C(arg1, temp, (s8)func_800D9340(arg0));
    if (arg2 == 1) {
        func_800DB500(1);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106518);

void func_80106550(GpActorWork* arg0)
{
    if (arg0->actor->field_97D & 4) {
        func_801055D4(arg0, 0, 0, 0);
    } else {
        func_80108770(arg0, 3);
    }
}

void func_801065A0(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801065A8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801066DC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106838);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106A3C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106C6C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010747C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010771C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801078AC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80107E1C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108084);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108224);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801083A0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108458);

void func_80108568(GpActorWork* arg0)
{
    GameActor* actor;

    actor = arg0->actor;
    if (actor->field_973 != actor->field_974) {
        func_80108770(arg0, 4);
    } else if (actor->field_973 == 0) {
        if (actor->field_975 != actor->field_976) {
            func_80108684(arg0);
        }
    }
}

void func_801085D0(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108620);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108684);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010870C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108770);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108874);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801088D4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108A0C);

void func_80108B80(GpActorWork* arg0)
{
    func_80105B0C(arg0);
    func_80105ED4(arg0);
}

void func_80108BAC(GpActorWork* arg0)
{
    func_80103AC0(arg0);
    func_80105ED4(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108BD8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108CC4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108D68);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108E0C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108E40);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108ED4);

void func_80108FA0(GpActorWork* arg0)
{
    func_801065A8(arg0);
    func_80109290(arg0);
    func_80105ED4(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108FD4);

void func_801090E8(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_973 = 0;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        func_8010870C(arg0, 4);
    }
}

void func_80109138(GpActorWork* arg0)
{
    func_8010615C(arg0);
    func_801041FC(arg0, 0);
    func_801093DC(arg0);
}

void func_80109170(GpActorWork* arg0)
{
    GameActor* inner;
    u8         kind;

    inner = arg0->actor;
    kind  = inner->field_972;
    switch (kind) {
        case 0:
        case 1:
        case 2:
        case 8:
        case 9:
        case 10:
        case 11:
            func_8010ABD4(arg0);
            break;
        case 5:
            func_8010AC54(arg0);
            break;
        case 6:
            func_80109A1C(arg0);
            break;
        case 3:
            func_8010AD64(arg0);
            break;
        case 7:
            func_80109844(arg0);
            break;
    }
}

void func_80109208(void)
{
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109210);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109250);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109290);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109374);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801093DC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801094D4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801095BC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109684);

void func_80109700(GpActorWork* arg0)
{
    func_80103AC0(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109720);

void func_80109818(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 4;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 5;
    inner->field_95E = 0;
    inner->field_981 = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109844);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109A1C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109BB4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109FC4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010A1B0);

void func_8010A42C(GpActorWork* arg0, s32 arg1)
{
    u8 kind;

    kind = arg1;
    if (kind != 0) {
        switch (kind) {
            case 0:
                break;
            case 1: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x101) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 1;
                inner->field_944     = 0x258;
                func_800EC9C8();
                func_80103B5C(arg0);
                func_800ECA10(1);
                break;
            }
            case 2: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x102) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 2;
                inner->field_946     = 0x258;
                inner->field_98E     = 0;
                func_8010B210(arg0);
                func_800ECA10(2);
                break;
            }
            case 3: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x104) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 4;
                inner->field_948     = 0x258;
                inner->field_98D     = 0;
                func_800ECA10(4);
                break;
            }
            case 4:
                func_800ECA10(8);
                break;
            case 8: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x108) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 0x10;
                inner->field_94A     = 0x258;
                func_800ECA10(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x110) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 0x20;
                inner->field_94C     = 0x258;
                func_800ECA10(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x120) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 0x40;
                inner->field_94E     = 0x258;
                inner->field_990     = (func_80037164() & 0x1F) + 0xA;
                inner->field_970     = 0;
                func_800ECA10(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x140) != 0) {
                    return;
                }
                D_80073B88.field_25 |= 0x80;
                inner->field_950     = 0x258;
                func_800ECA10(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010A670);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010A854);

void func_8010A9D0(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    func_8010AAB4(arg0);
    if ((u16)inner->field_96C == 1) {
        mode = 0x10;
    } else {
        mode = 0x11;
    }
    func_80103A18(arg0, mode, 0, 3);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010AA28);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010AAB4);

void func_8010AB70(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    if (inner->field_956 != 0) {
        func_8010870C(arg0, 0xC);
    } else {
        func_801066DC(arg0, 0);
    }
}

void func_8010ABD4(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_95E != 0) {
        if (inner->field_95E == 1) {
            func_8010B210(arg0);
            inner->field_97A = 0x12;
            if (inner->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010AC54);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010AD64);

void func_8010AE98(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x101) != 0) {
        return;
    }
    D_80073B88.field_25 |= 1;
    inner->field_944     = 0x258;
    func_800EC9C8();
    func_80103B5C(arg0);
    func_800ECA10(1);
}

void func_8010AF04(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x102) != 0) {
        return;
    }
    D_80073B88.field_25 |= 2;
    inner->field_946     = 0x258;
    inner->field_98E     = 0;
    func_8010B210(arg0);
    func_800ECA10(2);
}

void func_8010AF6C(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x104) != 0) {
        return;
    }
    D_80073B88.field_25 |= 4;
    inner->field_948     = 0x258;
    inner->field_98D     = 0;
    func_800ECA10(4);
}

void func_8010AFC0(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x108) != 0) {
        return;
    }
    D_80073B88.field_25 |= 0x10;
    inner->field_94A     = 0x258;
    func_800ECA10(0x10);
}

void func_8010B010(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x110) != 0) {
        return;
    }
    D_80073B88.field_25 |= 0x20;
    inner->field_94C     = 0x258;
    func_800ECA10(0x20);
}

void func_8010B060(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x120) != 0) {
        return;
    }
    D_80073B88.field_25 |= 0x40;
    inner->field_94E     = 0x258;
    inner->field_990     = (func_80037164() & 0x1F) + 0xA;
    inner->field_970     = 0;
    func_800ECA10(0x40);
}

void func_8010B0C8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x140) != 0) {
        return;
    }
    D_80073B88.field_25 |= 0x80;
    inner->field_950     = 0x258;
    func_800ECA10(0x80);
    func_800ECA54();
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B120);

void func_8010B210(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_96C = 0;
    inner->field_972 = 0;
    inner->field_96E = 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B228);

void func_8010B2A0(s32 arg0, s32 arg1)
{
    func_8002CFA0(D_80113340, arg0, arg1, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B2D4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B348);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B3F8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B520);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B590);

void func_8010B5C0(Task* arg0)
{
    Task*         parent;
    GameActorExt* extra;

    parent          = arg0->parent;
    extra           = (GameActorExt*)arg0->extra;
    extra->field_C  = ((GameActorExt*)parent->extra)->field_C;
    *extra->field_8 = 0;
}

void func_8010B5E4(Task* arg0)
{
    arg0->state = 3;
}

void func_8010B5F0(Task* arg0)
{
    Task_Kill(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B610);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B674);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B79C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B9A4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BAC8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BC04);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BC70);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BCF4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BD88);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BE5C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BF7C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BFCC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C058);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C098);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C180);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C1FC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C30C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C46C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C4F0);

s32 func_8010C648(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &D_80073B88;
    saved = p->field_24;
    func_80104B54();
    p->field_24 = saved;
    return 0;
}

s32 func_8010C688(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &D_80073B88;
    saved = p->field_24;
    func_80104E00();
    p->field_24 = saved;
    return 0;
}

s32 func_8010C6C8(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &D_80073B88;
    saved = p->field_24;
    func_80105070();
    p->field_24 = saved;
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C708);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C75C);

void func_8010C81C(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &D_80073B88;
    saved = p->field_24;
    func_801053A0();
    p->field_24 = saved;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C858);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C8F0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C980);
