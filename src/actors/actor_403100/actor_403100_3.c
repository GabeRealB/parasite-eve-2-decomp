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

void func_actor_403100_8013D564(Task* arg0, s32 arg1, u16* arg2)
{
    u16 value;

    switch (arg2[1]) {
        case 10:
            arg0->state                        = 3;
            D_actor_403100_80155808->field_5F8 = 0;
            D_actor_403100_80155808->field_5FA = 0;
            break;
        case 0xFFFF:
            D_actor_403100_80155808->field_5F8 = 0;
            D_actor_403100_80155808->field_5FA = 0;
            arg0->state                        = 2;
            break;
        default:
            value = arg2[1];
            if (value < 9U) {
                D_actor_403100_80155808->field_5F8 = value;
                D_actor_403100_80155808->field_5FA = 0;
                arg0->state                        = 1;
            }
            break;
    }
    D_actor_403100_80155808->field_5FA = 0;
}
void func_actor_403100_8013D5F4(void)
{
    D_actor_403100_80155808->field_65F = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013D608);
void func_actor_403100_8013D6B4(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                              = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_606 = D_actor_403100_80155808->field_604;
    D_actor_403100_80155808->field_60A = D_actor_403100_80155808->field_608;
    D_actor_403100_80155808->field_A8  = D_actor_403100_80155808->field_A0;
    D_actor_403100_80155808->field_AA  = D_actor_403100_80155808->field_A2;
    D_actor_403100_80155808->field_AC  = D_actor_403100_80155808->field_A4;
    D_actor_403100_80155808->field_5D4 = coord[6].coord.t[0];
    D_actor_403100_80155808->field_5F0 = D_actor_403100_80155808->field_5EE;
}
void func_actor_403100_8013D700(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                              = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_604 = D_actor_403100_80155808->field_606;
    D_actor_403100_80155808->field_608 = D_actor_403100_80155808->field_60A;
    D_actor_403100_80155808->field_A0  = D_actor_403100_80155808->field_A8;
    D_actor_403100_80155808->field_A2  = D_actor_403100_80155808->field_AA;
    D_actor_403100_80155808->field_A4  = D_actor_403100_80155808->field_AC;
    coord[6].coord.t[0]                = D_actor_403100_80155808->field_5D4;
    D_actor_403100_80155808->field_5EE = D_actor_403100_80155808->field_5F0;
}
void func_actor_403100_8013D74C(Task* arg0)
{
    D_actor_403100_80155808->field_5EE             = 0;
    D_actor_403100_80155808->field_664.b.field_667 = 0;
    D_actor_403100_80155808->field_638             = 0;
    D_actor_403100_80155808->field_63A             = 0;
}
void func_actor_403100_8013D770(Task* arg0)
{
    SVECTOR           rotation;
    Actor403100Matrix matrix;
    MATRIX*           dest;
    MATRIX*           mtx;
    GsCOORDINATE2*    coords;
    GsCOORDINATE2*    updated;

    coords               = ((TmdObject*)arg0->extra)->field_8;
    dest                 = &coords[6].coord;
    coords[6].flg        = 0;
    mtx                  = &matrix.mat;
    matrix.ident.m00_m01 = 0x1000;
    matrix.ident.m02_m10 = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matrix.ident.m20_m21 = 0;
    mtx->m[2][2]         = 0x1000;
    Gp_MtxToEuler(dest, &rotation);
    USE_REG(mtx);
    rotation.vz += D_actor_403100_80155808->field_A4;
    rotation.vy += D_actor_403100_80155808->field_A2;
    rotation.vx += D_actor_403100_80155808->field_A0;
    RotMatrix(&rotation, &matrix.mat);
    dest->m[0][0] = matrix.mat.m[0][0];
    dest->m[0][1] = matrix.mat.m[0][1];
    dest->m[0][2] = matrix.mat.m[0][2];
    dest->m[1][0] = matrix.mat.m[1][0];
    dest->m[1][1] = matrix.mat.m[1][1];
    dest->m[1][2] = matrix.mat.m[1][2];
    dest->m[2][0] = matrix.mat.m[2][0];
    dest->m[2][1] = matrix.mat.m[2][1];
    updated       = coords + 6;
    USE_REG(updated);
    dest->m[2][2] = matrix.mat.m[2][2];
    Gp_UpdateCoord(updated);
}
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

void func_actor_403100_8013DAC4(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131F60;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DB48(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = D_actor_403100_80131F84;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
        func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
        func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
    }
}
void func_actor_403100_8013DC18(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80131F9C;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DCAC(Task* arg0)
{
    TaskFuncTable9 sp;

    sp = D_actor_403100_80131FB0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_80132C3C(arg0, 6, 7, 0x400, -0xC80);
    func_actor_403100_80132C3C(arg0, 7, 8, 0x400, -0xC80);
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_3", func_actor_403100_8013DD78);
void func_actor_403100_8013DE0C(Task* arg0)
{
    TaskFuncTable5 sp;

    sp = D_actor_403100_80132000;
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    }
}
void func_actor_403100_8013DEA0(Task* arg0)
{
    TaskFuncTable4 sp;

    sp = D_actor_403100_80132014;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013DF0C(void)
{
    void (*fns[2])(void) = { func_actor_403100_8013F610, func_actor_403100_8013F658 };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}
void func_actor_403100_8013DF64(void)
{
    void (*fns[2])(void) = { (void (*)(void))func_actor_403100_8013B128, (void (*)(void))func_actor_403100_8013B3C4 };

    fns[(s16)D_actor_403100_80155808->field_5FA]();
}
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

void func_actor_403100_8013E174(void)
{
    s16 timer;

    if (D_80073BA0 > 0) {
        timer                              = (u16)D_actor_403100_80155808->field_5F4 - 1;
        D_actor_403100_80155808->field_5F4 = timer;
        if (timer < 0) {
            func_actor_403100_8013D1B8(5, 0x3F4);
            D_actor_403100_80155808->field_5F2 = 2;
            return;
        }
        func_actor_403100_8013D1B8(5, 0x3F4);
    }
}
void func_actor_403100_8013E1E4(void)
{
    GpAnimArg sp;

    if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
        D_actor_403100_8015570C.sets[4] = ((Actor403100AnimTable*)Gp_PlayerAnimBlkTbl[Gp_WeaponIdBase[D_8007218A - 1] + D_80073BA9])->sets[7];
        sp.field_0                      = &D_actor_403100_8015570C;
        sp.field_8                      = 1;
        sp.field_C                      = 3;
        sp.field_10                     = 0;
        sp.field_4                      = 4;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&sp, 0);
        D_actor_403100_80155808->field_5F2 = 3;
    }
}
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
void func_actor_403100_8013E624(Task* arg0)
{
    u16 timer;

    timer                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = timer;
    if ((s16)timer == 0x12) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x168) {
        arg0->state                        = 5;
        D_actor_403100_80155808->field_5F8 = 0;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
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
void func_actor_403100_8013EA60(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj          = arg0->extra;
    sp           = D_actor_403100_80131EC8;
    obj->field_C = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
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
void func_actor_403100_8013EB68(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_actor_403100_80131EE4;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
}
void func_actor_403100_8013EBC8(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj          = arg0->extra;
    sp           = D_actor_403100_80131EF0;
    obj->field_C = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
void func_actor_403100_8013EC4C(Task* arg0)
{
    TaskFuncTable4 sp;
    TmdObject*     obj;

    obj          = arg0->extra;
    sp           = D_actor_403100_80131F00;
    obj->field_C = 0;
    sp.funcs[(s16)D_actor_403100_80155808->field_5FA](arg0);
    func_actor_403100_801327CC(arg0);
}
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

void func_actor_403100_8013EE28(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_61C = 2;
    D_actor_403100_80155808->field_5DE = 3;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_801327CC(arg0);
    D_actor_403100_80155808->field_82   = 0;
    coord->coord.t[0]                   = -0x1710;
    coord->coord.t[1]                   = 0;
    coord->coord.t[2]                   = -0x2846;
    D_actor_403100_80155808->field_618  = 0x1910;
    D_actor_403100_80155808->field_5EC  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013EEB0(void)
{
}

void func_actor_403100_8013EEB8(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                               = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_5EC += 1;
    coord->coord.t[2]                  += 0x64;
    if ((s16)D_actor_403100_80155808->field_5EC == 0x20) {
        D_actor_403100_80155808->field_5EC  = 0;
        D_actor_403100_80155808->field_5FA += 1;
    }
    func_actor_403100_801327CC();
}
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

void func_actor_403100_8013F0A8(Task* arg0)
{
    TmdObject* obj;

    obj                               = (TmdObject*)arg0->extra;
    obj->field_C                     |= 0x80;
    D_actor_403100_8014762C.field_8   = 0;
    D_actor_403100_8014762C.field_A   = 0;
    D_actor_403100_8014762C.field_6 >>= 1;
    Game_Session->field_9             = 4;
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    arg0->state                        = 5;
    D_actor_403100_80155808->field_5F8 = 0;
    D_actor_403100_80155808->field_5FA = 0;
}
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

void func_actor_403100_8013F1D8(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
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

void func_actor_403100_8013F3EC(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord                              = ((TmdObject*)arg0->extra)->field_8;
    D_actor_403100_80155808->field_5F6 = 5;
    coord->coord.t[0]                  = -0x44C;
    coord->coord.t[1]                  = -0x1388;
    coord->coord.t[2]                  = 0x2710;
    D_actor_403100_80155808->field_82  = 0xA00;
    D_actor_403100_80155808->field_5E2 = 0x10;
    D_actor_403100_80155808->field_5DE = 0xB;
    D_actor_403100_80155808->field_80  = 0;
    D_actor_403100_80155808->field_84  = 0;
    D_actor_403100_80155808->field_5DA = 2;
    func_actor_403100_80132528(arg0);
    Mc_SaveData.field_4                 = 0x18;
    D_actor_403100_80155808->field_60E  = 0;
    D_actor_403100_80155808->field_610  = 0;
    D_actor_403100_80155808->field_5FA += 1;
}
void func_actor_403100_8013F488(void)
{
    if (Actor403100_TestFlags()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
void func_actor_403100_8013F4E0(void)
{
    u16 frame;

    frame                              = D_actor_403100_80155808->field_5EC;
    D_actor_403100_80155808->field_5EC = frame + 1;
    if ((s16)frame >= 0xD) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
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
void func_actor_403100_8013F588(void)
{
    if ((func_actor_403100_80133928() << 0x10) == 0) {
        D_actor_403100_80155808->field_5F6             = 4;
        D_actor_403100_80155808->field_5E2             = 0xC;
        D_actor_403100_80155808->field_61C             = 2;
        D_actor_403100_80155808->field_5DE             = 8;
        D_actor_403100_80155808->field_5DA             = 2;
        D_actor_403100_80155808->field_5EC             = 0;
        D_actor_403100_80155808->field_82              = (u16)D_actor_403100_80155808->field_82 & 0xFFF;
        D_actor_403100_80155808->field_668.b.field_668 = 0;
        D_actor_403100_80155808->field_668.b.field_669 = 0;
        D_actor_403100_80155808->field_5FA            += 1;
    }
}
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

void func_actor_403100_8013F658(void)
{
    if (Actor403100_TestFlags104()) {
        D_actor_403100_80155808->field_5F8 = 1;
        D_actor_403100_80155808->field_5FA = 0;
    }
}
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
