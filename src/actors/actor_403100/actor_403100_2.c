#include "common.h"
#include "psyq/inline_c.h"

#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

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

void                        func_8017E3C8(void);
void                        func_8017E4B8(void);
extern u16                  D_actor_403100_80147630;
extern GsCOORDINATE2*       D_actor_403100_80155630;
extern u32                  D_actor_403100_801556EC[];
extern u32                  D_actor_403100_8015572C[];
extern Actor403100QuadEntry D_actor_403100_801557E0[2];

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
void func_80182730(void);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_403100_801326DC(Actor403100Work* work);
void func_actor_403100_8013712C(Task* arg0);
void func_actor_403100_8013C008(s16 arg0, s16 arg1);
void func_actor_403100_8013D74C(Task* arg0);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);
void func_actor_403100_8013F12C(void);

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

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137268);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137310);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801375B8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801376D8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801379B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137CA8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137DC4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80137F4C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138048);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013842C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138610);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801386DC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138790);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138844);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138AB4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138C18);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138D08);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138DB0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80138F88);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013922C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_801395EC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80139818);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_80139E80);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A064);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A254);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A4C8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A5AC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013A81C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013AA04);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013AC04);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013AE28);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013B128);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013B3C4);
extern MATRIX Gfx_ViewWorldMtx;

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013B5E0);
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131F84);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131F9C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131FB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80131FD4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132000);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132014);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132024);

extern TaskFuncTable6 D_actor_403100_80132030;

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100_2", D_actor_403100_80132030);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013BA64);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013BB8C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013BDE4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013BEF0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C008);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C214);
static inline s32 Actor403100CoordToViewInline(GsCOORDINATE2* coord, SVECTOR* pos, GsCOORDINATE2* view)
{
    SVECTOR        local;
    VECTOR         result;
    s32            flag;
    GsCOORDINATE2* current;

    current  = coord;
    local.vx = pos->vx;
    local.vy = pos->vy;
    local.vz = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return 0;
        }
        if (current == view) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return 1;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        __asm__ volatile("nop; nop; .word 0x4A480012");
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

static inline void Actor403100ResetStateInline(s16 anim, s16 angle, s16 frame)
{
    D_actor_403100_80155808->field_5FC = frame;
    D_actor_403100_80155808->field_5E2 = angle;
    D_actor_403100_80155808->field_5DE = anim;
    D_actor_403100_80155808->field_5DA = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013C7B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013CBE0);
void func_actor_403100_8013CDC0(void)
{
    s16 next;
    s16 next2;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_666;
    switch (state) { /* irregular */
        case 0:
            D_actor_403100_80155808->field_5EA =
                (u16)D_actor_403100_80155808->field_5EA +
                ((s32) - (D_actor_403100_80155808->field_5EA * 0x10) >> 7);
            return;
        case 1:
            D_actor_403100_80155808->field_664.b.field_666 = 2;
            return;
        case 2:
            next = (u16)D_actor_403100_80155808->field_5EA +
                   ((s32)(0x1E00 - (D_actor_403100_80155808->field_5EA * 0x10)) >> 7);
            D_actor_403100_80155808->field_5EA = next;
            if (next >= 0x1C0) {
                D_actor_403100_80155808->field_664.b.field_666 =
                    D_actor_403100_80155808->field_664.b.field_666 + 1;
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5EA - 0xC;
            D_actor_403100_80155808->field_5EA = next2;
            if ((next2 << 0x10) <= 0) {
                D_actor_403100_80155808->field_664.b.field_666 = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013CEAC);
void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    s16 facing;
    s16 target;
    u16 yaw;
    u16 targetU;
    u16 facingU;

    yaw = (u16)arg0->vy;
    if ((u32)((yaw + 0x27F) & 0xFFFF) < 0x4FFU) {
        target  = D_actor_403100_80155808->field_B2;
        targetU = (u16)D_actor_403100_80155808->field_B2;
        if ((u32)(((s16)yaw - target) + 0x20) >= 0x41U) {
            if (target < (s16)yaw) {
                D_actor_403100_80155808->field_B2 = targetU + arg1;
                D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg2;
                return;
            }
            D_actor_403100_80155808->field_B2 = targetU - arg1;
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg2;
            return;
        }
        facing  = (s16)D_actor_403100_80155808->field_82;
        facingU = D_actor_403100_80155808->field_82;
        if (facing < target) {
            D_actor_403100_80155808->field_82 = facingU + arg3;
            return;
        }
        if (target < facing) {
            D_actor_403100_80155808->field_82 = facingU - arg3;
        }
    } else {
        if ((s16)yaw >= 0x281) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg4;
        }
        if (arg0->vy < -0x280) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg4;
        }
    }
}

void func_actor_403100_8013D06C(void)
{
    if (D_actor_403100_80155808->field_B4 >= 0x11) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 - 8;
    }
    if (D_actor_403100_80155808->field_B4 < -0x10) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 + 8;
    }
}

void func_actor_403100_8013D0B8(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    Actor403100MsgPos msg;

    msg.pos.vx = arg0;
    msg.pos.vy = arg1;
    msg.pos.vz = arg2;
    msg.rot.vx = 0;
    msg.rot.vy = arg3;
    msg.rot.vz = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&msg, 0);
}
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013D11C);
void func_actor_403100_8013D1B8(s16 arg0, s16 arg1)
{
    GpAnimArg msg;

    msg.field_0                        = &D_actor_403100_8015570C;
    msg.field_4                        = (s32)arg0;
    msg.field_8                        = 0;
    msg.field_C                        = 0;
    msg.field_10                       = 0;
    D_actor_403100_80155808->field_65D = (s8)arg0;
    if (arg1 == 0x3FF) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&msg, 0);
    } else if (arg1 == 0x3F4) {
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F4, (s32)&msg, 0);
    }
}
void func_actor_403100_8013D24C(void)
{
    s32 state;

    state = (s8)D_actor_403100_80155808->pad_66A[2];
    if ((state == 1) && !(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        D_actor_403100_80155808->field_664.b.field_665 = state;
        D_actor_403100_80155808->field_664.b.field_666 = (u8)state;
        D_actor_403100_80155808->field_66F             = state;
    }
}
void func_actor_403100_8013D2A0(s16 arg0)
{
    if (!(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        if (arg0 == 1) {
            D_actor_403100_80155808->field_66F = 2;
        }
        D_actor_403100_80155808->field_664.b.field_665 = 1;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100_2", func_actor_403100_8013D2F4);