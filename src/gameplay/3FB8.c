#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/task.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

extern TaskDesc       D_80113340[];
extern GpEffArg       D_80113358;
extern TaskFuncTable3 D_800977FC;
extern u16            D_80112D68[];
extern GpAnimBlk*     D_80112D6C[];
extern u16            D_80112DF4[];
extern u16            D_80113360[];
extern void*          D_80113368[];

s32  func_8010A854(s32 arg0);
void func_80101848(GpActorWork* arg0);
void func_80101A68(GpActorWork* arg0);
void func_80101F58(GpActorWork* arg0);
void func_80102348(GpActorWork* arg0, s32 arg1);
void func_80102634(GpActorWork* arg0);
void func_801029D4(GpActorWork* arg0);
void func_80102D20(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_801038F8(GpActorWork* arg0, s32 arg1);
void func_8010397C(GpActorWork* arg0, s32 arg1, s32 arg2);
void func_80103A18(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_80103AC0(GpActorWork* arg0);
s16  func_80103E7C(s16 arg0, s16 arg1);
void func_80103F70(GpActorWork* arg0);
void func_80104B54(void);
void func_80104E00(void);
s32  func_80105070(void);
void func_801053A0(void);
s32  func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_80105B0C(GpActorWork* arg0);
s32  func_80105ED4(GpActorWork* arg0);
void func_8010615C(GpActorWork* arg0);
void func_801066DC(GpActorWork* arg0, s16 arg1);
void func_80107E1C(GpActorWork* arg0);
void func_80108684(GpActorWork* arg0);
void func_8010870C(GpActorWork* arg0, s32 arg1);
void func_80108770(GpActorWork* arg0, s32 arg1);
void func_80108874(GpActorWork* arg0);
void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1);
void func_80109290(GpActorWork* arg0);
void func_80109374(GpActorWork* arg0);
void func_801093DC(GpActorWork* arg0);
void func_801095BC(s32* arg0);
void func_80109720(GpActorWork* arg0);
void func_80109844(GpActorWork* arg0);
void func_80109A1C(GpActorWork* arg0);
void func_80109BB4(GpActorWork* arg0, GpRec18* arg1);
void func_8010AA28(GpActorWork* arg0, s32 arg1);
void func_8010AAB4(GpActorWork* arg0);
void func_8010ABD4(GpActorWork* arg0);
void func_8010AC54(GpActorWork* arg0);
void func_8010AD64(GpActorWork* arg0);
void func_8010B120(GpActorWork* arg0);
void func_800FDB18(s32 arg0, GsCOORDINATE2* arg1, SVECTOR* arg2, GpEffArg* arg3);

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

void func_800FC6F4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800977FC;
    sp.funcs[arg0->state](arg0);
}

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
    D_80115760[0]  = NULL;
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

void func_801014E8(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097848;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010154C);

void func_80101848(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimRec* rec;
    s32        i;
    s32        anim;
    s32        extra;
    u16        flags;

    actor = arg0->actor;
    rec   = func_800B4668((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->pad_438 + 1);
    switch (actor->field_95C) {
        case 0:
        case 1:
            break;
        case 2:
            if (actor->field_95E != 0) {
                break;
            }
            if (func_8010583C(arg0, 0, 0, 0) != 0) {
                break;
            }
            anim              = 9;
            extra             = 5;
            i                 = 1;
            actor->field_95E += i;
            actor             = arg0->actor;
            if (i < actor->field_938) {
                do {
                    func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, anim, 0, 0, extra,
                                  actor->field_928);
                    ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 =
                        actor->field_985;
                    i++;
                } while (i < actor->field_938);
            }
            break;
        case 3:
            break;
        case 5:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_80108770(arg0, 3);
                }
            }
            break;
        case 4:
        case 6:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 7:
        case 9:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E++;
                }
            }
            break;
        case 8:
            if (rec != NULL) {
                flags = actor->field_448[1].field_0;
                if ((flags & 1) || (flags & 2)) {
                    actor->field_95E++;
                    func_801066DC(arg0, 0);
                }
            }
            break;
        case 10:
            if (rec != NULL) {
                if (func_80105894(arg0, 1, 0, 0) == 0) {
                    actor->field_95E = 0x3E8;
                }
            }
            break;
    }
}

static const s32 s_jtbl_pad = 0;

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

Task* func_801036FC(GpActorArg* arg0, u16 arg1, s32 arg2, GpActorFlags* arg3)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;

    task = Task_Spawn(7, Wip_SysConfig.field_26 + 3, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
    Task_Kill(task);
    return NULL;

have_actor:
    Game_SetPtrSlot(task, 3);
    task->idMap = (TaskIdMap*)actor;
    Mem_Set(actor, 0, 0x998);
    actor->field_93C  = arg3->field_0;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    D_80115768        = 0;
    if (arg3->field_2 != 0) {
        actor->field_954 = 2;
    }
    return task;
}

void func_80103804(GpActorWork* arg0)
{
    GameActor* actor;
    u16        buttons;
    s32        flag;

    actor            = arg0->actor;
    actor->field_974 = actor->field_973;
    actor->field_976 = actor->field_975;
    actor->field_964 = actor->field_962;
    buttons          = Game_Session->field_58;
    actor->field_978 = actor->field_977;
    actor->field_962 = buttons;
    actor->field_966 = actor->field_962 & ~actor->field_964;
    actor->field_968 = actor->field_964 & ~actor->field_962;
    flag             = 1;
    actor->field_977 = (actor->field_962 >> 6) & flag;
}

void func_80103874(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21;
    actor->field_928 = D_80112D6C[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->pad_438);
}

void func_801038F8(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B3FA8((GpAnimCtx*)actor->field_424, i, arg1);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_8010397C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, 0, actor->field_928);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_80103A18(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    i     = 1;
    if (i < actor->field_938) {
        do {
            func_800B47A8((GpAnimCtx*)actor->field_424, i, 0, arg1, 0, 0, arg3, actor->field_928);
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = actor->field_985;
            i++;
        } while (i < actor->field_938);
    }
}

void func_80103AC0(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            func_800B4514((GpAnimCtx*)inner->field_424, i);
            i++;
        } while (i < inner->field_938);
    }
}

s32 func_80103B1C(void)
{
    WipSysConfig* p;
    s32           temp;
    s32           ret;

    p    = &Wip_SysConfig;
    temp = (u16)p->field_1a << 16;
    if ((temp >> 17) < p->field_18) {
        ret = 0;
    } else {
        ret = 1;
        if ((temp >> 18) >= p->field_18) {
            ret = 2;
        }
    }
    return ret;
}

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

void func_80103C74(GsCOORDINATE2* arg0, VECTOR3* arg1, VECTOR3* arg2)
{
    arg2->vx = arg1->vx - arg0->coord.t[0];
    arg2->vy = arg1->vy - arg0->coord.t[1];
    arg2->vz = arg1->vz - arg0->coord.t[2];
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103CB4);

s32 func_80103D8C(s32 arg0, s32 arg1)
{
    arg0 = ABS(arg0);
    arg0 = arg0 * arg0;
    arg1 = ABS(arg1);
    arg1 = arg1 * arg1;
    return SquareRoot0(arg0 + arg1);
}

s32 func_80103DD4(VECTOR3* arg0, VECTOR3* arg1)
{
    void**       scratch;
    u8*          head;
    VECTOR3*     vec;
    s32          vz;
    register s32 absz asm("v1");
    register s32 vx asm("a0");

    scratch                       = (void**)G_SCRATCH_HEAD;
    head                          = *scratch;
    ((VECTOR3*)(head - 0x10))->vx = arg0->vx - arg1->vx;
    vec                           = (VECTOR3*)(head - 0x10);
    vec->vy                       = arg0->vy - arg1->vy;
    vz                            = arg0->vz - arg1->vz;
    absz                          = ABS(vz);
    vec->vz                       = vz;
    absz                          = absz * absz;
    vx                            = ((VECTOR3*)(head - 0x10))->vx;
    vx                            = ABS(vx);
    vx                            = vx * vx;
    *scratch                      = vec;
    vx                            = SquareRoot0(vx + absz);
    *scratch                      = (u8*)*scratch + 0x10;
    return vx;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80103E7C);

void func_80103F70(GpActorWork* arg0)
{
    GameActor*    actor;
    GpLinkNode*   node;
    WipSysConfig* p;
    s32           val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL) {
        actor->field_97E = 1;
        return;
    }
    if (node->field_4 & 1) {
        node->field_5    = 0;
        actor->field_90C = NULL;
        actor->field_97E = 1;
        return;
    }
    if ((s8)actor->field_97E == 2) {
        p = &Wip_SysConfig;
        if (p->field_21 == 0x17) {
            val = 0x200;
        } else {
            val = 0x180;
        }
        func_80102348(arg0, val);
        if (p->field_21 == 0x17) {
            func_801029D4(arg0);
        } else {
            func_80102634(arg0);
        }
    }
}

GsCOORDINATE2* func_8010403C(s32 arg0)
{
    Task* slot;
    u8    idx;

    slot = Game_GetPtrSlot(3);
    idx  = D_80112E2C[Mc_SaveData.field_22 - 1][arg0];
    return &((GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8)[idx];
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801040A0);

s32 func_801041B4(GpActorWork* arg0)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    for (i = 0; i < 0x12; i++) {
        if ((actor->field_17C[i].field_4 & 0x100100) == 0x100000) {
            return 1;
        }
    }
    return 0;
}

void func_801041FC(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor;
    GpPadEvt*  entry;
    u8         count;
    s32        idx;

    actor = arg0->actor;
    count = actor->field_981;
    if ((s8)actor->field_981 == 0) {
        idx = arg1 & 0xFFFF;
        asm("");
        actor->field_981 = count + 1;
        entry            = &D_80112E28[idx];
        Pad_PostEvent(0, 1, entry->field_0, entry->field_2);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104258);

Task* func_80104364(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*         task;
    s32*          saved;
    GameActorExt* extra;
    GpCoordExt*   coord;
    s32           type;

    saved = arg0->extra->field_8;
    if (arg2 == 0) {
        return NULL;
    }
    type = D_80112DF4[arg1] - 1;
    task = Task_Spawn(7, type + arg2, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra           = (GameActorExt*)task->extra;
    task->parent    = (Task*)arg0;
    coord           = (GpCoordExt*)extra->field_8;
    coord->sub      = saved;
    coord->field_44 = 1;
    return task;
}

s32 func_801043F4(void)
{
    GpActorWork* work;
    GameActor*   actor;
    Task*        task;

    work  = Game_GetPtrSlot(3);
    actor = work->actor;
    if (!work | !actor) {
        return 0;
    }

    task = actor->field_918;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_918 = NULL;
    }

    task = actor->field_91C;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_91C = NULL;
    }

    task = actor->field_914;
    if (task != NULL) {
        Task_Kill(task);
        actor->field_914 = NULL;
    }

    func_800E1638((GpObj*)actor->field_10C);
    return 1;
}

Task* func_80104490(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*          task;
    GsCOORDINATE2* saved;
    GameActorExt*  extra;

    saved  = (GsCOORDINATE2*)((GameActorExt*)arg0->actor->field_91C->extra)->field_8;
    arg2 <<= 2;
    arg1  += 0x60;
    task   = Task_Spawn(7, arg2 + arg1, arg3, 0);
    if (task == NULL) {
        return NULL;
    }
    extra                                 = (GameActorExt*)task->extra;
    task->parent                          = (Task*)arg0;
    ((GsCOORDINATE2*)extra->field_8)->sub = saved;
    return task;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104508);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104684);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104838);

void func_80104A4C(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor       = arg0->actor;
    p           = &Wip_SysConfig;
    p->field_24 = 0;
    if (actor->field_954 != 2) {
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (actor->field_966 & 0x20) {
                    p->field_24 = 1;
                }
            }
        }
    }
}

void func_80104AAC(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104B54);

s32 func_80104CAC(GpActorWork* arg0, s32 arg1, GpAnimArg* arg2)
{
    GameActor* actor;
    GpAnimObj* extra;
    s32        flag;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_928 = arg2->field_0;
    actor->field_93A = 0x7FFF;
    actor->field_985 = 0x10;
    if (arg2->field_8 == 0) {
        func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                      (GpAnimSlot*)actor->pad_438);
        func_801038F8(arg0, arg2->field_4);
    } else {
        func_80103A18(arg0, arg2->field_4, 0, arg2->field_C);
    }
    flag = arg2->field_10;
    if (flag == 0) {
        flag = 0x38;
    } else {
        flag = 7;
    }
    actor->field_983 = flag;
    return 0;
}

s32 func_80104D68(Task* arg0, s32 arg1, GpXformArg* arg2)
{
    GameActorExt*  extra;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    extra             = (GameActorExt*)arg0->extra;
    actor             = (GameActor*)arg0->idMap;
    coord             = (GsCOORDINATE2*)extra->field_8;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    coord->coord.t[2] = arg2->field_8;
    actor->field_50   = arg2->field_10;
    actor->field_52   = arg2->field_12;
    actor->field_54   = arg2->field_14;
    mtx               = &coord->coord;
    RotMatrix((SVECTOR*)&actor->field_50, mtx);
    MatrixNormal(mtx, mtx);
    coord->flg = 0;
    func_80098F58(coord);
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104E00);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80104F5C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105070);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105190);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801052B8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801053A0);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801054D8);

s32 func_801055D4(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 0xA;
    actor->field_983 = 0x38;
    return 0;
}

s32 func_80105690(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor*    actor;
    WipSysConfig* p;

    actor             = arg0->actor;
    p                 = &Wip_SysConfig;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    p->field_24       = 0;
    actor->field_97E  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, p->field_21, 0);
    if (Game_Session->field_1 != 0) {
        ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
    }
    actor->field_956 = 7;
    actor->field_934 = arg2;
    return 0;
}

s32 func_80105754(GpActorWork* arg0)
{
    GameActor*    actor;
    WipSysConfig* p;
    s32           ret;

    actor = arg0->actor;
    ret   = 0;
    if (actor->field_954 != 2) {
        p                 = &Wip_SysConfig;
        actor->field_954  = 2;
        actor->field_95E  = 0;
        actor->field_973  = 0;
        actor->field_975  = 0;
        p->field_24       = 0;
        actor->field_97E  = 0;
        actor->field_60   = 0;
        actor->field_58   = 0;
        actor->field_64   = 0;
        actor->field_5C   = 0;
        actor->field_6A   = 0;
        actor->field_68   = 0;
        actor->field_70   = 0;
        actor->field_96C  = 0;
        actor->field_12A &= 0x3FFF;
        func_80106350(arg0, p->field_21, ret);
        if (Game_Session->field_1 != 0) {
            ((GpObj*)actor->field_AC)->flags &= 0xDFFF;
        }
        actor->field_956 = 0xB;
    } else {
        ret = 1;
    }
    return ret;
}

s32 func_80105828(GpActorWork* arg0)
{
    return arg0->actor->field_982;
}

s32 func_8010583C(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;
    s32        i;
    s32        ret;

    actor = arg0->actor;
    ret   = 0;
    for (i = actor->field_938 - 1; i > 0; i--) {
        if ((actor->field_448[i].field_0 & 0x100) == 0) {
            ret = 1;
            break;
        }
    }
    return ret;
}

s32 func_80105894(GpActorWork* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GameActor* actor;

    actor = (GameActor*)((arg1 * sizeof(GameActorSlot)) + (s32)arg0->actor);
    return (actor->field_448[0].field_0 & 0x102) == 0;
}

s32 func_801058BC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        i;

    actor = arg0->actor;
    if (arg2 <= 0) {
        arg2 = 1;
    } else if (arg2 >= 0x80) {
        arg2 = 0x7F;
    }
    i = 1;
    if (i < actor->field_938) {
        do {
            ((GameActor*)((i * sizeof(GameActorSlot)) + (s32)actor))->field_441 = arg2;
            i++;
        } while (i < actor->field_938);
    }
    actor->field_985 = arg2;
    return 0;
}

s32 func_80105914(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)D_80112D6C[D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 func_801059AC(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        ret;
    s32        out;

    actor = arg0->actor;
    ret   = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        ret = func_8010A854((s16)func_800E2438(arg2, 0, &out, 0));
        if (ret != 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
        } else if (actor->field_910 == 0) {
            func_8010A42C(arg0, (u8)out);
        }
    }
    return ret;
}

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

s32 func_80105AB0(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;

    inner = arg0->actor;
    if (arg2 == 0) {
        inner->field_987 = 1;
        inner->field_98A = 2;
        inner->field_988 = 0;
        inner->field_98B = 0;
        inner->field_989 = 0;
        inner->field_98C = 0;
    } else if (arg2 < 4) {
        inner->field_987 = arg2 + 1;
        inner->field_988 = 0;
        inner->field_989 = 0;
    } else {
        inner->field_98A = arg2 - 3;
        inner->field_98B = 0;
        inner->field_98C = 0;
    }
    return 0;
}

void func_80105B0C(GpActorWork* arg0)
{
    GameActor* inner;
    s32        i;

    inner = arg0->actor;
    i     = 1;
    if (i < inner->field_938) {
        do {
            func_800B3DF4((GpAnimCtx*)inner->field_424, (GpAnimSlot*)inner->pad_438 + i);
            i++;
        } while (i < inner->field_938);
    }
}

void func_80105B74(VECTOR3* arg0)
{
    GameActor* actor;

    actor           = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_40 = arg0->vx;
    actor->field_44 = arg0->vy;
    actor->field_48 = arg0->vz;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105BC4);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80105ED4);

s32 func_801060E0(GpActorWork* arg0)
{
    GameActor* actor;
    u16        mode;
    s32        flags;
    s32        mask1;
    s32        mask2;

    actor = arg0->actor;
    mode  = actor->field_954;
    if (mode != 2) {
        flags = actor->field_962;
        mask1 = 8;
        mask2 = 2;
    } else {
        flags = ((PadState*)&Pad_States)->buttons;
        if (Mc_SaveData.field_1a8 == mode) {
            mask1 = 0x80;
            mask2 = 0x10;
        } else {
            mask1 = 8;
            mask2 = 2;
        }
    }
    actor->field_97F = 0;
    if (flags & mask1) {
        actor->field_97F = 1;
    } else if (flags & mask2) {
        actor->field_97F = 2;
    }
    return actor->field_97F;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010615C);

void func_801061F0(void)
{
    WipSysConfig* p;
    GpActorWork*  work;
    s32           flag;
    register s32  f21 asm("a1");
    s32           f22;

    work                   = Game_GetPtrSlot(3);
    p                      = &Wip_SysConfig;
    flag                   = 0x20000;
    f21                    = p->field_21;
    f22                    = p->field_22;
    work->actor->field_124 = (f21 << 8) | (f22 | flag);
}

void func_80106238(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;

    actor            = arg0->actor;
    actor->field_124 = (actor->field_124 & 0xFFFF3FFF) | (((arg1 << 1) | arg2) << 14);
}

s32 func_80106264(s32 arg0)
{
    s32 item;
    s32 ret;

    item = Wip_SysConfig.field_21 + 0x7F;
    ret  = 0;
    if (arg0 & 1) {
        ret = func_800B6DA4(item, 0);
    }
    if (arg0 & 2) {
        ret |= func_800B6DA4(item, 0x100) << 16;
    }
    return ret;
}

s32 func_801062DC(GpActorWork* arg0, s32 arg1)
{
    s32 ret;
    s32 flag;
    s32 item;

    ret  = 0;
    item = Wip_SysConfig.field_21;
    flag = arg1 != 1;
    if (func_800BB418(item + 0x7F, flag) == 1) {
        func_801088D4(arg0, flag, ret);
        ret = 1;
    }
    return ret;
}

void func_80106350(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* actor;
    s32        value;

    actor = arg0->actor;
    if (arg1 == 0x16) {
        if (actor->field_914 != NULL) {
            actor->field_914->spawnArg1 = -1;
        }
        SndEvt_EnqueueType7(0x20160003, 0);
        SndEvt_EnqueueType7(0x20160004, 0);
        SndEvt_EnqueueType7(0x20160005, 0);
    } else if (arg1 == 0x19) {
        if (actor->field_914 != NULL) {
            if (func_800B6DA4(0x98, 0x100) != 0) {
                actor->field_914->spawnArg1 = 1;
            } else {
                actor->field_914->spawnArg1 = 0;
            }
        }
    } else if (arg1 == 0x1C) {
        if (actor->field_914 != NULL) {
            if (func_800B6DA4(0x9B, 0x100) != 0) {
                value = 1;
                if (actor->field_914->spawnArg1 == 2) {
                    value = 3;
                }
                actor->field_914->spawnArg1 = value;
            } else {
                actor->field_914->spawnArg1 = (actor->field_914->spawnArg1 == 2) << 2;
            }
            if (actor->field_910 == NULL) {
                SndEvt_EnqueueType7(0x201C0005, 0);
            } else {
                SndEvt_EnqueueType7(0x40680002, 0);
            }
        }
    }
    actor->field_97E  = 1;
    actor->field_12A &= 0x3FFF;
}

void func_801064A4(GpObj38* arg0, s32 arg1, s32 arg2)
{
    s32 temp;

    temp = (s8)func_800D937C(arg0);
    SndEvt_EnqueueType6(arg1, temp, (s8)func_800D9340(arg0));
    if (arg2 == 1) {
        func_800DB500(1);
    }
}

void func_80106518(s32 arg0)
{
    register s32 cap asm("v0");
    s32          idx;

    cap = 0x1869E;
    idx = arg0 - 1;
    if (Mc_SaveData.field_888[idx] <= cap) {
        Mc_SaveData.field_888[idx]++;
    }
}

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

void func_801065A8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    func_80109374(arg0);
    if (inner->field_97D & 1) {
        inner->field_97C = 1;
        func_8010870C(arg0, 5);
    } else if (inner->field_973 != inner->field_974 ||
               (inner->field_977 != inner->field_978 && inner->field_973 == 1)) {
        func_801066DC(arg0, 0);
    } else if (inner->field_973 == 0 && inner->field_975 != inner->field_976) {
        func_80108620(arg0);
    } else if ((inner->field_962 & 0xF000) == 0) {
        if (inner->field_942 < 0x7FFF) {
            inner->field_942++;
            if (inner->field_942 == 0x12C) {
                func_80103A18(arg0, func_80103B1C() + 0x17, 0, 5);
            }
        }
    }
}

void func_801066DC(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    temp             = inner->field_973;
    inner->field_956 = 0;
    inner->field_95C = 0;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            if (inner->field_975 == 1) {
                mode = 6;
            } else {
                mode = 5;
            }
        } else {
            mode = 1;
        }
        inner->field_958 = 0;
        inner->field_95A = 3;
    } else if ((inner->field_962 & 0x40) && (temp != -1)) {
        temp             = 1;
        inner->field_95A = temp;
        if (Mc_SaveData.field_25 == 0 && inner->field_991 == 0) {
            inner->field_958 = 3;
            mode             = 4;
        } else {
            mode             = 2;
            inner->field_958 = temp;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
        }
    } else {
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            if (Mc_SaveData.field_25 != 0 && inner->field_991 == 0) {
                inner->field_958 = 3;
                mode             = 4;
            } else {
                inner->field_958 = 1;
                mode             = 2;
                if (inner->field_91C == NULL) {
                    mode = 0x13;
                }
            }
        } else {
            inner->field_958 = 2;
            mode             = 3;
        }
    }
    inner->field_954 = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (arg1 != 0) {
        func_801038F8(arg0, mode);
    } else {
        func_80103A18(arg0, mode, 0, 4);
    }
}

INCLUDE_RODATA("gameplay/nonmatchings/3FB8", D_80097940);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106838);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106A3C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80106C6C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010747C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010771C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_801078AC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80107E1C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80108084);

void func_80108224(GpActorWork* arg0)
{
    GameActor* actor;
    GameActor* inner;
    s32        mode;
    s32        temp;
    s32        flag;

    actor = arg0->actor;
    switch (actor->field_95E) {
        case 0:
            flag             = 1;
            actor->field_95E = flag;
            func_80103A18(arg0, 0x28, 0, 6);
            break;
        case 1:
            if (func_800B4668((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->pad_438 + 1) !=
                NULL) {
                if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                    inner            = arg0->actor;
                    inner->field_954 = 0;
                    inner->field_956 = 2;
                    inner->field_958 = 0;
                    if (inner->field_973 != 0) {
                        temp = 1;
                    } else {
                        temp = 3;
                    }
                    inner->field_95A = temp;
                    inner->field_95C = 0;
                    inner->field_95E = 0;
                    if (Wip_SysConfig.field_25 & 1) {
                        func_80103B5C(arg0);
                        inner->field_97E = 1;
                    } else {
                        inner->field_97E = 2;
                    }
                    temp = inner->field_973;
                    if (temp == 0) {
                        if (inner->field_975 != 0) {
                            mode = 0xD;
                        } else {
                            mode = 9;
                        }
                    } else if (temp == 1) {
                        mode             = 0xC;
                        inner->field_958 = 3;
                        inner->field_97E = temp;
                    } else {
                        inner->field_958 = 2;
                        mode             = 0xD;
                    }
                    func_80103A18(arg0, mode, 0, 4);
                }
            }
            break;
    }
    func_80103AC0(arg0);
    func_8010B120(arg0);
}

void func_801083A0(GpActorWork* arg0)
{
    GameActor*        inner;
    GpActorFuncTable3 sp;

    sp    = D_80097940;
    inner = arg0->actor;
    func_80104A4C(arg0);
    if (inner->field_940 > 0) {
        inner->field_940--;
    }
    if ((s8)inner->field_97A > 0) {
        inner->field_97A--;
    }
    inner->field_986 = 0;
    sp.funcs[inner->field_954](arg0);
    func_80109720(arg0);
    func_801030CC(arg0);
}

void func_80108458(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    inner            = arg0->actor;
    node             = func_800DAD54(arg0);
    inner->field_973 = 0;
    if ((node != NULL && D_801153F0.field_0 < 2) || (flag = 1, D_801153F0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            func_800DB4E0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            func_80108770(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            func_80103B5C(arg0);
            func_80108874(arg0);
        }
    }
}

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

void func_80108620(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 3;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    if (inner->field_975 == 0) {
        mode = 1;
    } else if (inner->field_975 == 1) {
        mode = 6;
    } else {
        mode = 5;
    }
    func_80103A18(arg0, mode, 0, 5);
}

void func_80108684(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (inner->field_973 != 0) {
        if (inner->field_973 == 1) {
            temp = 3;
        } else {
            temp = 2;
        }
        mode             = 0xD;
        inner->field_958 = temp;
        inner->field_95A = 1;
        if (inner->field_973 == 1) {
            mode = 0xC;
        }
    } else {
        mode             = 0xD;
        inner->field_95A = 3;
        if (inner->field_975 == 0) {
            mode = 9;
        }
    }
    func_80103A18(arg0, mode, 0, 5);
}

void func_8010870C(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 1;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 2;
    inner->field_95E = 0;
    inner->field_940 = 0;
    if (arg1 == 0) {
        func_801038F8(arg0, 7);
    } else {
        func_80103A18(arg0, 7, 0, arg1);
    }
}

void func_80108770(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;
    s32        mode;
    s32        temp;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 2;
    inner->field_958 = 0;
    if (inner->field_973 != 0) {
        temp = 1;
    } else {
        temp = 3;
    }
    inner->field_95A = temp;
    inner->field_95C = 0;
    inner->field_95E = 0;
    if (Wip_SysConfig.field_25 & 1) {
        func_80103B5C(arg0);
        inner->field_97E = 1;
    } else {
        inner->field_97E = 2;
    }
    temp = inner->field_973;
    if (temp == 0) {
        if (inner->field_975 != 0) {
            mode = 0xD;
        } else {
            mode = 9;
        }
    } else if (temp == 1) {
        mode             = 0xC;
        inner->field_958 = 3;
        inner->field_97E = temp;
    } else {
        inner->field_958 = 2;
        mode             = 0xD;
    }
    if (arg1 == 0) {
        func_801038F8(arg0, mode);
    } else {
        func_80103A18(arg0, mode, 0, arg1);
    }
}

void func_80108874(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_956 = 3;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 2;
    inner->field_95C = 4;
    inner->field_95E = 0;
    func_80103A18(arg0, 8, 0, 6);
    func_80103B5C(arg0);
}

void func_801088D4(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    if (arg2 == 2) {
        if (func_80106264(arg1) != 0) {
            if (D_80112F1C[Wip_SysConfig.field_21][0] == 0) {
                inner->field_95E = 0x3E8;
                return;
            }
        }
        inner->field_95C = 0xA;
        mode             = 0x14;
        if (Mc_SaveData.field_13 == 1) {
            func_80166E94(Game_GetPtrSlot(0xA), 0);
        }
    } else {
        if (arg2 == 1) {
            if (inner->field_954 == 2) {
                return;
            }
        }
        inner->field_95C = 5;
        mode             = arg1 + 0xE;
    }
    inner->field_956 = 5;
    inner->field_954 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95E = 0;
    inner->field_960 = arg1;
    inner->field_93E = arg2;
    func_80106350(arg0, Wip_SysConfig.field_21, 0);
    func_80103A18(arg0, mode, 0, 3);
}

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

void func_80108BD8(GpActorWork* arg0)
{
    GameActor* inner;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    inner = arg0->actor;
    cur   = inner->field_52;
    tgt   = inner->field_82;
    raw   = inner->field_82;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x41 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x41)) {
        flag             = 1;
        inner->field_52  = raw;
        inner->field_982 = 0;
        inner->field_956 = flag;
        func_80103A18(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x40) {
            delta = 0x40;
        } else if (delta < -0x40) {
            delta = -0x40;
        }
        inner->field_52 = ((u16)inner->field_52 + delta) & 0xFFF;
    }
    func_80103AC0(arg0);
}

void func_80108CC4(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
        case 1:
            func_80107E1C(arg0);
            if (inner->field_95E == 2) {
                inner->field_958 = 3;
                mode             = 4;
                if (inner->field_93C != 0) {
                    mode = inner->field_93C;
                }
                func_80103A18(arg0, mode, 0, 5);
            }
            break;
        case 2:
            func_80107E1C(arg0);
            break;
    }
}

void func_80108D68(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (inner->field_93E >= inner->field_934) {
        inner->field_97A = 0x12;
        if (inner->field_95E == 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            inner->field_95E = 1;
        }
    } else if (inner->field_966 & 0xF0F0) {
        inner->field_93E++;
    }
    func_80103AC0(arg0);
}

void func_80108E0C(GpActorWork* arg0, GpLinkNode* arg1)
{
    GameActor*  inner;
    GpLinkNode* node;

    inner = arg0->actor;
    node  = inner->field_90C;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        inner->field_90C = arg1;
    }
    arg1->field_5 = 1;
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_800979F8\n"
        "D_800979F8:\n"
        "\t.word func_80109170\n"
        "\t.word func_80109170\n"
        "\t.word func_80109170\n"
        "\t.word func_80109208\n"
        ".section .text\n");
#endif

void func_80108E40(GpActorWork* arg0)
{
    GpActorFuncTable4 sp;

    sp = D_800979F8;
    sp.funcs[(u16)arg0->actor->field_96C](arg0);
    func_80101848(arg0);
    func_80103AC0(arg0);
    func_80101F58(arg0);
    func_80101A68(arg0);
}

#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 2\n"
        "\t.globl D_80097A08\n"
        "D_80097A08:\n"
        "\t.word func_80108B80\n"
        "\t.word func_80108BAC\n"
        "\t.word func_80108BD8\n"
        "\t.word func_801078AC\n"
        "\t.word func_80107E1C\n"
        "\t.word func_801094D4\n"
        "\t.word func_80108D68\n"
        "\t.word func_80109684\n"
        "\t.word func_80108CC4\n"
        "\t.word func_80109700\n"
        "\t.word func_80108084\n"
        "\t.word func_80108224\n"
        ".section .text\n");
#endif

void func_80108ED4(GpActorWork* arg0)
{
    GameActor*         inner;
    GpActorFuncTable12 sp;

    sp    = D_80097A08;
    inner = arg0->actor;
    sp.funcs[inner->field_956](arg0);
    func_80101F58(arg0);
    if (Wip_SysConfig.field_18 <= 0 && inner->field_956 != 0xA) {
        func_80103874(arg0);
        func_8010AA28(arg0, 4);
    }
}

void func_80108FA0(GpActorWork* arg0)
{
    func_801065A8(arg0);
    func_80109290(arg0);
    func_80105ED4(arg0);
}

void func_80108FD4(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         flag;

    func_80103F70(arg0);
    inner            = arg0->actor;
    node             = func_800DAD54(arg0);
    inner->field_973 = 0;
    if ((node != NULL && D_801153F0.field_0 < 2) || (flag = 1, D_801153F0.field_0 == flag) ||
        Mc_SaveData.field_929 != 0) {
        if (inner->field_95E != 0) {
            func_800DB4E0(1);
            if (inner->field_97C != 0) {
                inner->field_97C = 0;
                if (node != NULL) {
                    func_80108E0C(arg0, node);
                }
            }
            func_80108770(arg0, 3);
        }
    } else {
        func_80109374(arg0);
        if (inner->field_97D & 2) {
            inner->field_97C = 0;
            inner->field_97E = flag;
            func_80103B5C(arg0);
            func_80108874(arg0);
        }
    }
}

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

void func_80109210(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0xA000) {
        if (flags & 0x8000) {
            inner->field_975 = -1;
        } else {
            inner->field_975 = 1;
        }
    } else {
        inner->field_975 = 0;
    }
}

void func_80109250(GpActorWork* arg0)
{
    GameActor* inner;
    u16        flags;

    inner = arg0->actor;
    flags = inner->field_962;
    if (flags & 0x5000) {
        if (flags & 0x4000) {
            inner->field_973 = -1;
        } else {
            inner->field_973 = 1;
        }
    } else {
        inner->field_973 = 0;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_80109290);

void func_80109374(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if ((inner->field_962 & 0x80) && (D_80114C08.field_3 == 0) && (Wip_SysConfig.field_21 != 0) &&
        (inner->field_991 == 0)) {
        inner->field_97D = 1;
    } else {
        inner->field_97D = 2;
    }
}

void func_801093DC(GpActorWork* arg0)
{
    register GameActor* inner asm("a1");
    GpLinkNode*         next;
    u16                 flags;

    inner = arg0->actor;
    if (inner->field_90C != NULL) {
        flags = inner->field_966;
        if (flags & 0x40) {
            func_80103B5C(arg0);
            return;
        }
        if (((inner->field_962 & 0x80) && (flags & 0xA000)) || (flags & 0x80)) {
            next = func_800DAD78(arg0);
            goto install;
        }
    } else if ((inner->field_966 & 0x80) && !(Wip_SysConfig.field_25 & 1)) {
        register GpActorWork* a asm("a0");

        a                = arg0;
        inner->field_97E = 2;
        next             = func_800DAD54(a);
        goto install;
    }
    return;

install: {
    register GpLinkNode* arg1 asm("a1");
    register GpLinkNode* node asm("v1");
    GameActor*           actor;
    s32                  flag;

    actor = arg0->actor;
    node  = actor->field_90C;
    arg1  = next;
    flag  = 1;
    if (node != arg1) {
        if (node != NULL) {
            node->field_5 = 0;
        }
        actor->field_90C = arg1;
    }
    arg1->field_5 = flag;
}
}

void func_801094D4(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;
    s32        arg2;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 2;
            flag             = 1;
            inner->field_95E = flag;
            inner->field_958 = flag;
            if (inner->field_91C == NULL) {
                mode = 0x13;
            }
            arg2 = 1;
            if (inner->field_934 == 0) {
                arg2 = 6;
            }
            func_8010397C(arg0, mode, arg2);
        case 1:
            if (func_80105ED4(arg0) != 0) {
                inner->field_93E--;
                if (inner->field_93E <= 0) {
                    inner->field_982 = 0;
                    inner->field_956 = 1;
                    func_80103A18(arg0, 1, 0, 5);
                }
            } else {
                inner->field_973 = 1;
                func_80101A68(arg0);
            }
            break;
    }
    func_80103AC0(arg0);
}

void func_801095BC(s32* arg0)
{
    WipSysConfig*          p;
    volatile WipSysConfig* vp;

    p = &Wip_SysConfig;
    if (p->field_21 == 0x1B) {
        *arg0 = func_800BAFE0(p->field_21 + 0x7F)->field_2 - 0x9F;
        if (*arg0 < 0) {
            *arg0 = 0xA;
        }
        *arg0 = (*arg0 - 0xA) << 24;
    } else {
        vp = p;
        if ((u32)(vp->field_22 - 0xA) < 6U) {
            *arg0 = ((vp->field_22 - 1) % 3) << 24;
            if (*arg0 < 0) {
                *arg0 = 0;
            }
        } else {
            *arg0 = 0;
        }
    }
}

void func_80109684(GpActorWork* arg0)
{
    GameActor* inner;
    s32        mode;
    s32        flag;

    inner = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            mode             = 0x20;
            flag             = 1;
            inner->field_95E = flag;
            if (inner->field_934 != 0) {
                mode = 0x21;
            }
            func_8010397C(arg0, mode, 1);
        case 1:
            func_80103AC0(arg0);
            break;
    }
    func_8010B120(arg0);
}

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

void func_8010A1B0(s32 arg0, s32 arg1)
{
    GpActorWork* work;
    GameActor*   inner;
    s32          mask;

    mask = arg1;
    if (arg0 == 0) {
        work = Game_GetPtrSlot(3);
        if (arg1 & 1) {
            inner = work->actor;
            if (func_800B9D80(0x101) == 0) {
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
                func_800EC9C8();
                func_80103B5C(work);
                func_800ECA10(1);
            }
        }
        if (mask & 2) {
            inner = work->actor;
            if (func_800B9D80(0x102) == 0) {
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
                func_8010B210(work);
                func_800ECA10(2);
            }
        }
        if (mask & 4) {
            inner = work->actor;
            if (func_800B9D80(0x104) == 0) {
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
                func_800ECA10(4);
            }
        }
        if (mask & 0x10) {
            inner = work->actor;
            if (func_800B9D80(0x108) == 0) {
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                func_800ECA10(0x10);
            }
        }
        if (mask & 0x20) {
            inner = work->actor;
            if (func_800B9D80(0x110) == 0) {
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                func_800ECA10(0x20);
            }
        }
        if (mask & 0x40) {
            inner = work->actor;
            if (func_800B9D80(0x120) == 0) {
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                func_800ECA10(0x40);
            }
        }
        if (mask & 0x80) {
            inner = work->actor;
            if (func_800B9D80(0x140) == 0) {
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                func_800ECA10(0x80);
                func_800ECA54();
            }
        }
    } else {
        Wip_SysConfig.field_25 &= ~arg1;
    }
}

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
                Wip_SysConfig.field_25 |= 1;
                inner->field_944        = 0x258;
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
                Wip_SysConfig.field_25 |= 2;
                inner->field_946        = 0x258;
                inner->field_98E        = 0;
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
                Wip_SysConfig.field_25 |= 4;
                inner->field_948        = 0x258;
                inner->field_98D        = 0;
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
                Wip_SysConfig.field_25 |= 0x10;
                inner->field_94A        = 0x258;
                func_800ECA10(0x10);
                break;
            }
            case 9: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x110) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x20;
                inner->field_94C        = 0x258;
                func_800ECA10(0x20);
                break;
            }
            case 10: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x120) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x40;
                inner->field_94E        = 0x258;
                inner->field_990        = (rand() & 0x1F) + 0xA;
                inner->field_970        = 0;
                func_800ECA10(0x40);
                break;
            }
            case 11: {
                GameActor* inner;

                inner = arg0->actor;
                if (func_800B9D80(0x140) != 0) {
                    return;
                }
                Wip_SysConfig.field_25 |= 0x80;
                inner->field_950        = 0x258;
                func_800ECA10(0x80);
                func_800ECA54();
                break;
            }
        }
    }
}

void func_8010A670(GpActorWork* arg0)
{
    GameActor*  inner;
    GpLinkNode* node;
    s32         left;
    s32         right;
    s32         pad;
    s32         bits;
    s32         timer;
    s32         next;
    s32         mode;
    s32         dir;

    inner            = arg0->actor;
    timer            = inner->field_990 - 1;
    inner->field_990 = timer;
    if ((s8)timer == 0) {
        left             = 0x8000;
        next             = (rand() & 0x1F) + 0xA;
        pad              = inner->field_962;
        inner->field_990 = next;
        bits             = pad & 0xF000;
        if (bits == left || bits == (right = 0x2000)) {
            if (!(inner->field_970 & 0x5000)) {
                if (rand() & 1) {
                    dir = 0x4000;
                } else {
                    dir = 0x1000;
                }
                inner->field_970 = dir;
            }
        } else {
            bits = pad & 0x5000;
            if (bits) {
                if (rand() & 4) {
                    inner->field_970 &= 0xAFFF;
                } else if (rand() & 1) {
                    inner->field_970 = left;
                } else {
                    inner->field_970 = right;
                }
            }
        }
        if (D_801153F0.field_0 == 1) {
            if (inner->field_90C != NULL) {
                if (rand() & 3) {
                    func_80103B5C(arg0);
                }
            } else {
                mode = inner->field_956;
                if (mode == 2 && !(Wip_SysConfig.field_25 & 1) && (rand() & 3)) {
                    node = func_800DAD54(arg0);
                    if (node != NULL) {
                        inner->field_97E = mode;
                        func_80108E0C(arg0, node);
                    }
                }
            }
        }
    }
    if (Game_Session->field_58 & 0xF000) {
        inner->field_962 |= inner->field_970;
    }
}

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

void func_8010AA28(GpActorWork* arg0, s32 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 1;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_96C = 3;
    if (arg1 == 0) {
        func_801038F8(arg0, 0x12);
    } else {
        func_80103A18(arg0, 0x12, 0, arg1);
    }
    func_80103B5C(arg0);
    inner->field_983 |= 0x18;
}

void func_8010AAB4(GpActorWork* arg0)
{
    GameActor*    inner;
    WipSysConfig* p;

    p                   = &Wip_SysConfig;
    inner               = arg0->actor;
    D_80114C08.field_6 |= 1;
    func_80106350(arg0, p->field_21, 0);
    if (p->field_18 > 0) {
        inner->field_954 = 1;
        inner->field_958 = 0;
        inner->field_95A = 0;
        inner->field_95C = 7;
        inner->field_95E = 0;
        inner->field_973 = 0;
        func_8010A854(inner->field_96E);
        inner->field_12A &= 0x3FFF;
        if ((s8)inner->field_97E == 2) {
            inner->field_97E = 1;
        }
        func_8010A42C(arg0, inner->field_972);
    }
}

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

void func_8010AC54(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;

    inner = arg0->actor;
    if (inner->field_95E == 0) {
        inner->field_95E = 1;
        inner->field_934 = 0;
        inner->field_93E = 0;
    }
    if (inner->field_934 == 0) {
        inner->field_93E++;
        if (inner->field_93E == 3) {
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
        } else {
            inner->field_934 = 5;
        }
        func_800EA478(
            0x600E0, &((GsCOORDINATE2*)arg0->extra->field_8)[4 - inner->field_93E], 0x320, 0);
    } else {
        inner->field_934--;
    }
}

void func_8010AD64(GpActorWork* arg0)
{
    void**              scratch;
    register u8*        head asm("t1");
    SVECTOR*            vec;
    register GameActor* inner asm("a0");
    GameActor*          inner2;
    GpEffArg*           params;
    GsCOORDINATE2*      coord;
    s32                 val;
    s32                 idx;

    scratch = (void**)G_SCRATCH_HEAD;
    __asm__ volatile("" : "+r"(scratch));
    head     = *scratch;
    params   = &D_80113358;
    vec      = (SVECTOR*)(head - 8);
    *scratch = vec;
    inner    = arg0->actor;
    switch (inner->field_95E) {
        case 0:
            idx                     = (s8)inner->field_993;
            inner->field_95E        = 1;
            coord                   = (GsCOORDINATE2*)((GpObj*)inner->field_AC)[idx].field_8;
            params->field_4         = 0xC0;
            params->field_6         = 2;
            D_80113358.field_0      = coord;
            ((SVECTOR*)head)[-1].vx = 0;
            val                     = 0;
            if ((s8)inner->field_993 == 0) {
                val = -0x190;
            }
            vec->vy = val;
            vec->vz = 0;
            func_800FDB18(2, D_80113358.field_0, vec, params);
            break;
        case 1:
            break;
        case 2:
            asm("" ::: "memory");
            inner2 = arg0->actor;
            func_8010B210(arg0);
            inner2->field_97A = 0x12;
            if (inner2->field_956 != 0) {
                func_8010870C(arg0, 0xC);
            } else {
                func_801066DC(arg0, 0);
            }
            break;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 8;
}

void func_8010AE98(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x101) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 1;
    inner->field_944        = 0x258;
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
    Wip_SysConfig.field_25 |= 2;
    inner->field_946        = 0x258;
    inner->field_98E        = 0;
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
    Wip_SysConfig.field_25 |= 4;
    inner->field_948        = 0x258;
    inner->field_98D        = 0;
    func_800ECA10(4);
}

void func_8010AFC0(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x108) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x10;
    inner->field_94A        = 0x258;
    func_800ECA10(0x10);
}

void func_8010B010(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x110) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x20;
    inner->field_94C        = 0x258;
    func_800ECA10(0x20);
}

void func_8010B060(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x120) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x40;
    inner->field_94E        = 0x258;
    inner->field_990        = (rand() & 0x1F) + 0xA;
    inner->field_970        = 0;
    func_800ECA10(0x40);
}

void func_8010B0C8(GpActorWork* arg0)
{
    GameActor* inner;

    inner = arg0->actor;
    if (func_800B9D80(0x140) != 0) {
        return;
    }
    Wip_SysConfig.field_25 |= 0x80;
    inner->field_950        = 0x258;
    func_800ECA10(0x80);
    func_800ECA54();
}

void func_8010B120(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* inner2;
    GpObj38*   obj;
    s32        mode;
    s32        snd;
    s32        temp;
    s32        temp2;

    inner = arg0->actor;
    obj   = (GpObj38*)arg0->extra->field_8;
    if (Mc_SaveData.field_5C2 != 0) {
        return;
    }
    if ((s8)inner->field_97A != 0) {
        return;
    }
    func_80109BB4(arg0, inner->field_17C);
    if ((u16)inner->field_96C == 0) {
        return;
    }
    inner2 = arg0->actor;
    func_8010AAB4(arg0);
    mode = 0x11;
    if ((u16)inner2->field_96C == 1) {
        mode = 0x10;
    }
    func_80103A18(arg0, mode, 0, 3);
    temp  = (s8)func_800D937C(obj);
    temp2 = (s8)func_800D9340(obj);
    snd   = 7;
    if ((u16)inner->field_96C == 1) {
        snd = 6;
    }
    SndEvt_EnqueueType6(snd, temp, temp2);
}

void func_8010B210(GpActorWork* arg0)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_96C = 0;
    inner->field_972 = 0;
    inner->field_96E = 0;
}

s32 func_8010B228(s32 arg0)
{
    WipSysConfig* p;
    u16           saved18;
    u16           saved1c;
    s32           out;
    s32           ret;

    p           = &Wip_SysConfig;
    saved18     = p->field_18;
    saved1c     = p->field_1c;
    ret         = func_8010A854((s16)func_800E2438(arg0, 0, &out, 0));
    p->field_18 = saved18;
    p->field_1c = saved1c;
    if (ret != 0) {
        Display_ReleaseRef();
    }
    return ret;
}

void func_8010B2A0(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(D_80113340, arg0, arg1, 0);
}

void func_8010B2D4(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    s32        out;
    s32        flag;

    inner = arg0->actor;
    flag  = inner->field_910 != 0;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        arg2             = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_96E = func_800E2438(arg1->field_4, 0, &out, flag);
        inner->field_972 = out;
    }
}

void func_8010B348(GpActorWork* arg0, GpIdRec* arg1, s32 arg2)
{
    GameActor* inner;
    u32        kind;

    inner = arg0->actor;
    kind  = (u16)arg1->field_4;
    if ((u16)inner->field_96C == 0) {
        inner->field_993 = arg2;
        if (kind == 2) {
            goto case24;
        }
        if (kind < 3) {
            goto do_call;
        }
        if (kind == 3) {
            goto case3;
        }
        if (kind != 4) {
            goto do_call;
        }
    case24:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 5;
        goto do_call;
    case3:
        arg2 = (u16)arg2;
        if (arg2 == 1) {
            inner->field_96C = arg2;
        } else {
            inner->field_96C = 2;
        }
        inner->field_972 = 0;
    do_call:
        inner->field_96E = func_800E2CD4(arg1->field_4, 0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B3F8);

void func_8010B520(Task* arg0)
{
    s32*           raw;
    Task*          slot;
    GameActorExt*  extra;
    GpEffArg*      params;
    GsCOORDINATE2* coords;

    params          = &D_80113358;
    slot            = Game_GetPtrSlot(3);
    extra           = slot->extra;
    raw             = extra->field_8;
    params->field_4 = 0xC0;
    coords          = &((GsCOORDINATE2*)raw)[3];
    params->field_0 = coords;
    params->field_6 = (u16)arg0->spawnArg1 + 1;
    func_800FDB18(2, coords, 0, params);
    Task_Kill(arg0);
}

void func_8010B590(Task* arg0)
{
    GameActorExt* extra;
    GpCoordExt*   coord;

    extra = (GameActorExt*)arg0->extra;
    coord = (GpCoordExt*)extra->field_8;
    arg0->state++;
    coord->flg = 0;
    if (coord->field_44 != 0) {
        extra->field_C = 0;
    }
}

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

void func_8010B610(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_80097AB0;
    sp.funcs[arg0->state](arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B674);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B79C);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010B9A4);

Task* func_8010BAC8(GpActorArg* arg0, u16 arg1, s32 arg2, u16* arg3)
{
    Task*          task;
    GameActor*     actor;
    GpActorD4*     block;
    GsCOORDINATE2* coord;
    s32            type;

    if (arg1 == 1) {
        type = Mc_SaveData.field_5C7 + 0x7F;
    } else {
        type = arg1 + 0x82;
    }
    task = Task_Spawn(7, type, arg2, (s32)arg3);
    if (task != NULL) {
        goto have_task;
    }
    return NULL;

have_task:
    actor = Mem_Calloc(0x998, 0);
    if (actor != NULL) {
        goto have_actor;
    }
fail:
    Task_Kill(task);
    return NULL;

have_actor:
    block = Mem_Calloc(0xD4, 0);
    if (block == NULL) {
        goto fail;
    }
    Game_SetPtrSlot(task, 0xA);
    Mem_Set(actor, 0, 0x998);
    Mem_Set(block, 0, 0xD4);
    task->idMap      = (TaskIdMap*)actor;
    actor->field_910 = block;
    func_800A9DF0(task);
    actor->field_93C  = *arg3;
    actor->field_52   = arg0->field_0;
    coord             = (GsCOORDINATE2*)((GameActorExt*)task->extra)->field_8;
    coord->coord.t[0] = arg0->field_4;
    coord->coord.t[1] = arg0->field_8;
    coord->coord.t[2] = arg0->field_C;
    return task;
}

void func_8010BC04(GpActorWork* arg0, s16 arg1)
{
    GameActor* inner;

    inner            = arg0->actor;
    inner->field_954 = 0;
    inner->field_956 = 0;
    inner->field_958 = 0;
    inner->field_95A = 0;
    inner->field_95C = 0;
    inner->field_95E = 0;
    inner->field_942 = 0;
    inner->field_93E = 0;
    inner->field_973 = 0;
    inner->field_975 = 0;
    if (arg1 != 0) {
        func_801038F8(arg0, 1);
    } else {
        func_80103A18(arg0, 1, 0, 4);
    }
}

s32 func_8010BC70(GsCOORDINATE2* arg0)
{
    void**        scratch;
    u8*           head;
    VECTOR3*      vec;
    GameActorExt* extra;
    s32           ret;

    extra    = (GameActorExt*)((Task*)Game_GetPtrSlot(3))->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    func_80103C74(arg0, (VECTOR3*)((GsCOORDINATE2*)extra->field_8)->coord.t, vec);
    ret      = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

s16 func_8010BCF4(Task* arg0, VECTOR3* arg1)
{
    void**              scratch;
    u8*                 head;
    VECTOR3*            vec;
    GameActorExt*       extra;
    register GameActor* actor asm("s2");
    s16                 ret;

    extra    = (GameActorExt*)arg0->extra;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (VECTOR3*)(head - 0x10);
    *scratch = vec;
    actor    = (GameActor*)arg0->idMap;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, vec);
    ret      = func_80103E7C(actor->field_52, ratan2(((VECTOR3*)(head - 0x10))->vx, vec->vz));
    *scratch = (u8*)*scratch + 0x10;
    return ret;
}

void func_8010BD88(GpActorWork* arg0, VECTOR3* arg1)
{
    void**         scratch;
    u8*            head;
    GpTurnScratch* vec;
    GameActorExt*  extra;
    GameActor*     actor;
    s32            val;

    extra   = arg0->extra;
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    vec = *scratch = (GpTurnScratch*)(head - 0x14);
    actor          = arg0->actor;
    func_80103C74((GsCOORDINATE2*)extra->field_8, arg1, (VECTOR3*)vec);
    vec->angle = ratan2(((GpTurnScratch*)(head - 0x14))->vx, vec->vz);
    val        = func_80103E7C(actor->field_52, vec->angle);
    vec->angle = val;
    if (val > 0x40) {
        vec->angle = 0x40;
    } else if (val < -0x40) {
        vec->angle = -0x40;
    }
    actor->field_52         = (actor->field_52 + vec->angle) & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010BE5C);

void func_8010BF7C(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    arg0->actor->field_910->field_C4 = arg1 + (arg2 & rand());
}

void func_8010BFCC(GpActorWork* arg0)
{
    GameActor* actor;
    GpAnimObj* extra;

    actor            = arg0->actor;
    extra            = (GpAnimObj*)arg0->extra;
    actor->field_93A = D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7;
    actor->field_928 = D_80113368[actor->field_93A];
    func_800B3F84((GpAnimCtx*)actor->field_424, actor->field_928, extra, &actor->field_7A8,
                  (GpAnimSlot*)actor->pad_438);
}

s32 func_8010C058(void)
{
    s32 ret;

    if (((s16)Mc_SaveData.field_6CA >> 1) < (s16)Mc_SaveData.field_6C8) {
        ret = 0;
    } else if (((s16)Mc_SaveData.field_6CA >> 2) >= (s16)Mc_SaveData.field_6C8) {
        ret = 2;
    } else {
        ret = 1;
    }
    return ret;
}

void func_8010C098(GpActorWork* arg0, s32 arg1)
{
    GameActor*  actor;
    GpLinkNode* node;
    s32         val;

    actor = arg0->actor;
    node  = actor->field_90C;
    if (node == NULL || (node->field_4 & 1)) {
        actor->field_90C = NULL;
        actor->field_97E = 1;
    } else if ((s8)actor->field_97E == 2) {
        if (arg1 & 1) {
            val = 0;
            if (arg1 != 1) {
                val = 0x380;
            }
            func_80102348(arg0, val);
        }
        if (arg1 & 2) {
            if (D_80113388[Mc_SaveData.field_5C7] != 0) {
                func_80102634(arg0);
            } else {
                func_80102D20(arg0, D_80167218[Mc_SaveData.field_5C7], 0x380);
            }
        }
    }
}

void func_8010C180(GpActorWork* arg0)
{
    GameActor* inner;
    GameActor* actor;

    inner = arg0->actor;
    func_8010B210(arg0);
    inner->field_97A = 0x12;
    actor            = arg0->actor;
    actor->field_954 = 0;
    actor->field_956 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_942 = 0;
    actor->field_93E = 0;
    actor->field_973 = 0;
    actor->field_975 = 0;
    func_80103A18(arg0, 1, 0, 4);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C1FC);

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C30C);

void func_8010C46C(GpActorWork* arg0)
{
    GameActor* actor;

    actor             = arg0->actor;
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3FB8", func_8010C4F0);

s32 func_8010C648(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104B54();
    p->field_24 = saved;
    return 0;
}

s32 func_8010C688(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80104E00();
    p->field_24 = saved;
    return 0;
}

s32 func_8010C6C8(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_80105070();
    p->field_24 = saved;
    return 0;
}

s32 func_8010C708(GpActorWork* arg0)
{
    WipSysConfig* p;
    u8            saved;
    GameActor*    actor;

    p     = &Wip_SysConfig;
    actor = arg0->actor;
    saved = p->field_24;
    func_80105070();
    p->field_24      = saved;
    actor->field_956 = 8;
    return 0;
}

s32 func_8010C75C(GpActorWork* arg0, s32 arg1, GpDelayArg* arg2)
{
    GameActor* actor;

    actor = arg0->actor;
    if ((s8)actor->field_97A != 0) {
        return 1;
    }
    actor->field_954  = 2;
    actor->field_95E  = 0;
    actor->field_973  = 0;
    actor->field_975  = 0;
    actor->field_60   = 0;
    actor->field_58   = 0;
    actor->field_64   = 0;
    actor->field_5C   = 0;
    actor->field_6A   = 0;
    actor->field_68   = 0;
    actor->field_70   = 0;
    actor->field_96C  = 0;
    actor->field_12A &= 0x3FFF;
    func_80106350(arg0, D_80167218[Mc_SaveData.field_5C7], 0);
    actor->field_956 = 6;
    actor->field_934 = arg2->field_14;
    actor->field_93E = 0;
    return 0;
}

void func_8010C81C(void)
{
    WipSysConfig* p;
    u8            saved;

    p     = &Wip_SysConfig;
    saved = p->field_24;
    func_801053A0();
    p->field_24 = saved;
}

s32 func_8010C858(GpActorWork* arg0, s32 arg1, GpCopyArg* arg2)
{
    s32* dest;
    s32* src;
    s32  i;
    s32  count;

    dest  = (s32*)D_80113368[D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7];
    src   = arg2->field_0;
    count = arg2->field_4;
    if (count >= 0x21) {
        return 1;
    }
    dest = ((GpAnimBlk*)dest)->field_BC;
    for (i = 0; i < arg2->field_4; i++) {
        dest[i] = src[i];
    }
    return 0;
}

s32 func_8010C8F0(GpActorWork* arg0, s32 arg1, s32 arg2)
{
    s32 ret;

    ret = 0;
    if (Mc_SaveData.field_5C2 == 0) {
        Mc_SaveData.field_6C8 -= func_800E2438(arg2, 0, 0, 1);
        if ((s16)Mc_SaveData.field_6C8 <= 0) {
            func_800AC464(Game_GetPtrSlot(4), 0x7DA, 0, 0x7DE);
            ret = 1;
        }
    }
    return ret;
}

void func_8010C980(void* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3, s32 arg4, s32 arg5)
{
    arg1->field_8  = arg0;
    arg1->field_C  = arg2;
    arg1->field_10 = 0;
    arg1->field_12 = 0;
    arg1->field_14 = 0;
    arg1->flags    = 1;
    arg1->field_18 = arg4 | 0x30000;
    arg1->field_1C = arg5;
    func_800E15AC(2, arg1);
    arg1->flags |= 0x8000;
    func_800E18E0(arg1->field_C, (s16)arg3, 0);
}
