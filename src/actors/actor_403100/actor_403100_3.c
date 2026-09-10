#include "common.h"
#include "psyq/inline_c.h"

#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_403100.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_403100_80131E70;
extern TaskFuncTable3 D_actor_403100_80131E7C;

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `field_8` is
/// this actor's `GsCOORDINATE2`.
extern Actor403100Work* D_actor_403100_80155808;
extern GpEnemy*         D_actor_403100_8015580C;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void                  func_8017E3C8(void);
void                  func_8017E4B8(void);
extern u16            D_actor_403100_80147630;
extern GsCOORDINATE2* D_actor_403100_80155630;
extern u32            D_actor_403100_801556EC[];
extern u32            D_actor_403100_8015572C[];

extern s8                   D_8007216C;
extern s8                   D_8007218A;
extern u8                   D_80073BA9;
extern Actor403100AnimTable D_actor_403100_8015570C;
extern GpAnimBlk*           Gp_PlayerAnimBlkTbl[];
extern u16                  Gp_WeaponIdBase[];

extern Actor403100Light D_80114FF8;
extern u32              Gp_LcgState;

extern s16 D_80073BA0;
extern u8  D_801153F4;
extern u8  D_80165FC0;

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8017E128(s32 arg0);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_403100_801326DC(Actor403100Work* work);
void func_actor_403100_8013712C(Task* arg0);
void func_actor_403100_8013C008(s16 arg0, s16 arg1);
void func_actor_403100_8013D74C(Task* arg0);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);
void func_actor_403100_8013F12C(void);

static __inline__ s16 Actor403100_TestFlags104(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->field_B8.legacy.flags_104.word & 0x102) {
        return 1;
    }
    return 0;
}

static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_12C & 0x100) {
        return 1;
    }
    return 0;
}

static __inline__ s16 Actor403100_TestFlags(void)
{
    if (D_actor_403100_80155808->flags_634.half & 1) {
        return 1;
    }
    if (D_actor_403100_80155808->flags_634.word & 0x102) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D564);
void func_actor_403100_8013D5F4(void)
{
    D_actor_403100_80155808->field_65F = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D608);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D6B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D700);
void func_actor_403100_8013D74C(Task* arg0)
{
    D_actor_403100_80155808->field_5EE             = 0;
    D_actor_403100_80155808->field_664.b.field_667 = 0;
    D_actor_403100_80155808->field_638             = 0;
    D_actor_403100_80155808->field_63A             = 0;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D770);
void func_actor_403100_8013D88C(Task* arg0)
{
    Gp_UnlinkObj(&D_actor_403100_80155808->field_47C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_414);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_55C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_594);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

void func_actor_403100_8013D8F4(Task* arg0)
{
    D_actor_403100_80155808->field_658 = -1;
    D_actor_403100_80155808->field_618 = 0x1400;
    Gp_StateC08.field_6                = Gp_StateC08.field_6 | 1;
    Game_Session->field_12C            = 0;
    D_actor_403100_8015580C->field_4C  = 0;
    Gp_SetLightMode(arg0->spawnArg2, 0);
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    Gp_UnlinkNode(&D_actor_403100_8015580C->node);
    func_800E8614((s32)&D_80165FC0, 0);
    arg0->state                        = 1;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
    D_actor_403100_80155808->field_5F8 = 9;
    D_actor_403100_80155808->field_5FA = 0;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D9C4);
void func_actor_403100_8013DA6C(void)
{
    void (*fns[2])(void) = { (void (*)(void))func_actor_403100_8013712C, func_actor_403100_8013F12C };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DAC4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DB48);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DC18);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DCAC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DD78);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DE0C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DEA0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DF0C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DF64);
void func_actor_403100_8013DFBC(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80132024;
    func_actor_403100_8013D24C();
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013E02C(s16 arg0, s16 arg1, s16 arg2)
{
    D_actor_403100_80155808->field_5FC = arg2;
    D_actor_403100_80155808->field_5E2 = arg1;
    D_actor_403100_80155808->field_5DE = arg0;
    D_actor_403100_80155808->field_5DA = 1;
}

void func_actor_403100_8013E04C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E70;
    sp.funcs[task->state](task);
}

void func_actor_403100_8013E0A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131E7C;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E0FC);

void func_actor_403100_8013E16C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E174);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E1E4);
void func_actor_403100_8013E2BC(void)
{
    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 2, 0);
        D_actor_403100_80155808->field_5F4             = 0;
        D_actor_403100_80155808->field_65D             = 0;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_5F2             = 0;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E33C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E450);
void func_actor_403100_8013E5FC(void)
{
    D_actor_403100_8015580C->field_54   = 0;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5F8 += 1;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013E624);
void func_actor_403100_8013E6A0(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord               = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    arg0->killCountdown = 0x5A;
    coord->sub          = &Gfx_ViewCoord;
    coord->flg          = 0;
    arg0->state         = arg0->state + 1;
    func_actor_403100_8013E6F0(arg0);
}

void func_actor_403100_8013E6F0(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            countdown;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (D_801153F4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            Task_Kill(arg0);
        }
    }
}

void func_actor_403100_8013E784(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        Task_Kill(arg0);
    }
}

void func_actor_403100_8013E7C8(Task* arg0)
{
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coord2;
    u16            countdown;

    coord2              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    arg0->killCountdown = 0x5A;
    coord2->sub         = &Gfx_ViewCoord;
    coord2->flg         = 0;
    arg0->state        += 1;
    coord               = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (D_801153F4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x80020400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            Task_Kill(arg0);
        }
    }
}

void func_actor_403100_8013E88C(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            countdown;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (D_801153F4 == 0) {
        coord->flg = 0;
        if (!(arg0->killCountdown & 7)) {
            Gp_SpawnEff(0x60095, coord, 0x20400, NULL);
        }
        countdown           = arg0->killCountdown - 1;
        arg0->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            arg0->killCountdown = 0;
            arg0->state         = arg0->state + 1;
            Task_Kill(arg0);
        }
    }
}

void func_actor_403100_8013E920(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        Task_Kill(arg0);
    }
}
void func_actor_403100_8013E964(void)
{
}

void func_actor_403100_8013E96C(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EB0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013E9D8(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EBC;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->field_61C);
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EA60);
void func_actor_403100_8013EAD4(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj          = arg0->extra;
    sp           = D_actor_403100_80131ED8;
    obj->field_C = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
    func_actor_403100_8013B5E0(arg0, D_actor_403100_80155808->field_61C);
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EB68);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EBC8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EC4C);
void func_actor_403100_8013ECD0(Task* arg0)
{
    TaskFuncTable3 sp;
    TmdObject*     obj;

    obj          = arg0->extra;
    sp           = D_actor_403100_80131F10;
    obj->field_C = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013ED48(void)
{
}

void func_actor_403100_8013ED50(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC();
    D_actor_403100_80155808->field_82  = 0;
    coord->coord.t[0]                  = -0x2710;
    coord->coord.t[1]                  = -0x258;
    coord->coord.t[2]                  = -0x2328;
    D_actor_403100_80155808->field_600 = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013EDDC(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame == 0x1E) {
        func_8017E128(1);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EE28);
void func_actor_403100_8013EEB0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EEB8);
void func_actor_403100_8013EF24(void)
{
}

void func_actor_403100_8013EF2C(void)
{
}

void func_actor_403100_8013EF34(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5EE = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013EF58(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013EF60);
void func_actor_403100_8013EFC0(void)
{
}

void func_actor_403100_8013EFC8(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj               = (TmdObject*)arg0->extra;
    obj->field_E      = 0x10;
    coord             = (GsCOORDINATE2*)obj->field_8;
    coord->coord.t[0] = -0x49C;
    coord->coord.t[2] = 0x1130;
    coord->coord.t[1] = 0;

    D_actor_403100_80155808->field_82  = 0xC00;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0x14;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F034(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord             = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->coord.t[0] = -0xAF0;
    coord->coord.t[1] = 0x300;
    coord->coord.t[2] = -0xE74;

    D_actor_403100_80155808->field_82  = 0x800;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0x18;
    D_actor_403100_80155808->field_5DA = 2;
    D_actor_403100_80155808->field_618 = 0x1400;
    D_actor_403100_80155808->field_600 = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5EE = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F0A8);
void func_actor_403100_8013F12C(void)
{
    u16 frame;

    if ((func_actor_403100_80133928() << 0x10) == 0) {
        frame                              = D_actor_403100_80155808->field_5EC + 1;
        D_actor_403100_80155808->field_5EC = frame;
        if ((s16)frame >= 0x1F) {
            D_actor_403100_80155808->field_5F8 = 1;
            D_actor_403100_80155808->field_5FA = 0;
        }
    }
}

void func_actor_403100_8013F18C(void)
{
    D_actor_403100_80155808->field_5FC = 4;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 1;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_62C = 0x20;
    D_actor_403100_80155808->field_5F6 = 0;
    D_actor_403100_80155808->field_61C = 0;
    D_actor_403100_80155808->field_5EC = 0;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F1D8);
void func_actor_403100_8013F230(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

void func_actor_403100_8013F270(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    D_actor_403100_80155808->field_5FC = 0x14;
    D_actor_403100_80155808->field_5E2 = 0x1C;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

void func_actor_403100_8013F2D8(void)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->field_61C             = 4;
        D_actor_403100_80155808->field_5F6             = 4;
        D_actor_403100_80155808->field_604             = 0;
        D_actor_403100_80155808->field_608             = 0;
        D_actor_403100_80155808->field_5FA             = D_actor_403100_80155808->field_5FA + 1;
    }
}

void func_actor_403100_8013F344(void)
{
    if (((func_actor_403100_80133928() << 0x10) == 0) &&
        (D_actor_403100_80155808->field_5F6 == 5)) {
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 4;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5DA = 2;
        D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
    }
}

void func_actor_403100_8013F3AC(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0x5A) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F3EC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F488);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F4E0);
void func_actor_403100_8013F520(void)
{
    D_actor_403100_80155808->field_5EC = 0;
    SndEvt_EnqueueType7(0x401F0004, 0xA);
    D_actor_403100_80155808->field_5FC  = 0x14;
    D_actor_403100_80155808->field_5E2  = 0x1C;
    D_actor_403100_80155808->field_5DE  = 3;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5FA += 1;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F588);
void func_actor_403100_8013F610(void)
{
    D_actor_403100_80155808->field_61C = 2;
    D_actor_403100_80155808->field_5F6 = 4;
    D_actor_403100_80155808->field_5FC = 8;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0xA;
    D_actor_403100_80155808->field_5DA = 1;
    D_actor_403100_80155808->field_5FA = D_actor_403100_80155808->field_5FA + 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F658);
void func_actor_403100_8013F6B0(void)
{
    D_actor_403100_80155808->field_61C  = 2;
    D_actor_403100_80155808->field_5F6  = 4;
    D_actor_403100_80155808->field_5FC  = 0xA;
    D_actor_403100_80155808->field_5E2  = 8;
    D_actor_403100_80155808->field_5DE  = 0xA;
    D_actor_403100_80155808->field_5DA  = 1;
    D_actor_403100_80155808->field_5FA += 1;
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013F6F4);
void func_actor_403100_8013F76C(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0x3D) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}

void func_actor_403100_8013F7AC(void)
{
}

void func_actor_403100_8013F7B4(void)
{
}

void func_actor_403100_8013F7BC(void)
{
}
