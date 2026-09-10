#include "actors/coord_to_view.h"
#include "common.h"
#include "psyq/inline_c.h"

#include "main/gfx.h"
#include "main/sound.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "main/session.h"
#include "gameplay/3A34.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_403100.h"
#include "actors/actor_403100_rotation.h"
#include "actors/actor_403100_regions.h"

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

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132064);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132320);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132528);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801326DC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801327CC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801328DC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132C3C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801331D4);
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013335C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133928);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801339EC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133C94);
void func_actor_403100_80133D88(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            frame;

    coord                              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0xA0) {
        coord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x82) {
        func_actor_403100_801345E0(arg0, arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x12C) {
        D_actor_403100_80155808->field_5D8 = 0x1600;
        coord->coord.t[0]                  = -0xA28;
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 0xC;
        D_actor_403100_80155808->field_5DA = 2;
        coord->coord.t[1]                  = 0x1390;
        coord->coord.t[2]                  = 0x1130;
        D_actor_403100_80155808->field_82  = -0x6B0;
        D_actor_403100_80155808->field_604 = -0x140;
        D_actor_403100_80155808->field_608 = -0x100;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5F8 = D_actor_403100_80155808->field_5F8 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133E88);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801342B4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801345E0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013480C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E70);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E7C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80134D50);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013506C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801351F8);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013539C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801354A0);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801355D4);
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

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801356F4);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013588C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801359DC);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135AE0);
static __inline__ s16 Actor403100_TestFlags12C(void)
{
    if (D_actor_403100_80155808->field_B8.legacy.flags_12C & 0x100) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135C00);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135F30);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136100);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013631C);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136610);
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EC8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131ED8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EE4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EF0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F00);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F10);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F1C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F34);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136830);
INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013712C);
INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F60);
