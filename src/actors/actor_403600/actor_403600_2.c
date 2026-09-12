#include "common.h"

#include "actors/actor_403600.h"
#include "main/sound.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_403600_801320A0;
extern GpEnemyTaskFuncTable3 D_actor_403600_801320EC;
extern Task*                 D_actor_403600_801606A8;
extern s32                   D_80070F70;

void func_actor_403600_801411D4(Actor403600* arg0, s32 arg1);
void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1);
void func_actor_403600_80141338(Actor403600* arg0);
void func_actor_403600_801414FC(Actor403600* arg0);
void func_actor_403600_8014161C(Actor403600* arg0);
void func_actor_403600_80141A34(Actor403600* arg0);
void func_actor_403600_801400BC(Actor403600* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801411D4);

void func_actor_403600_801412D0(Actor403600Ctx* arg0, Actor403600* arg1)
{
    Actor403600Work* work;
    VECTOR*          head;
    VECTOR*          block;

    work                      = arg1->field_1C;
    head                      = *(VECTOR**)G_SCRATCH_HEAD;
    head[-1].vx               = work->field_4B8.workm.t[0];
    block                     = (VECTOR*)((u8*)head - 0x10);
    block->vy                 = work->field_4B8.workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = work->field_4B8.workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0, block, 0, 0);
    *(VECTOR**)G_SCRATCH_HEAD = (VECTOR*)((u8*)*(VECTOR**)G_SCRATCH_HEAD + 0x10);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141338);

void func_actor_403600_801414FC(Actor403600* arg0)
{
    Actor403600Work* work;
    s16              value;
    s16              countdown;
    s32              brightness;

    work  = arg0->field_1C;
    value = work->field_766;
    if (value != 0) {
        if (value < work->field_764) {
            brightness = rsin(D_80070F70 << 9) << 0xD;
        } else {
            brightness = rsin(D_80070F70 << 9) << 0xC;
        }
        Display_ClampField126((s8)(brightness >> 0x18));
        countdown       = (u16)work->field_764 - 1;
        work->field_764 = countdown;
        if ((countdown << 0x10) <= 0) {
            work->field_766 = 0;
            Display_ClampField126(0);
        }
    }
}

void func_actor_403600_80141598(Task* task)
{
    Actor403600Work* work;
    GpEnemy*         enemy;

    enemy                                   = task->spawnArg2;
    work                                    = (Actor403600Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->sub = &Gfx_ViewCoord;
    enemy->field_54                         = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_508);
    Gp_UnlinkObj(&work->field_588);
    if (task == D_actor_403600_801606A8) {
        Gp_UnlinkObj(&work->field_5C0);
    }
    Gp_EnemyTaskExit(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_8014161C);

void func_actor_403600_8014174C(Actor403600* arg0)
{
    Actor403600Work* work;

    work            = arg0->field_1C;
    work->field_756 = 8;
    work->field_778 = 0x10;
    work->field_776 = 0xA;
    work->field_742 = 0;
    work->field_746 = 0;
    work->field_774 = 0;
    work->field_77A = 0;
    work->field_784 = 0;
    work->field_73C = 0;
    work->field_73E = 0;
    work->field_74A = 0;
    work->field_73A = 0;
    work->field_76E = 0x40;
    work->field_75E = 0;
    work->field_7A4 = 0;
    work->field_7A6 = 0;
    work->field_7AC = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_801417A8);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141840);

void func_actor_403600_80141954(s32 arg0)
{
    RECT rect;

    rect.y = 0x80;
    rect.h = 0x80;
    rect.w = 0x80;
    if (arg0 == 1) {
        rect.x = 0x180;
    } else {
        rect.x = 0x1C0;
    }
    MoveImage(&rect, 0x180, 0x180);
    rect.w = 0x100;
    rect.h = 1;
    rect.x = 0;
    if (arg0 == 1) {
        rect.y = 0xFD;
    } else {
        rect.y = 0xFE;
    }
    MoveImage(&rect, 0, 0xF9);
}

void func_actor_403600_801419E8(Actor403600* arg0)
{
    TmdObject* obj;

    obj                  = arg0->field_2C;
    *(s8*)&obj->field_24 = -0xF;
    obj->field_25        = 2;
    if (obj->field_18 != NULL) {
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141A34);

void func_actor_403600_80141B24(Actor403600* arg0)
{
    Actor403600Work* work = arg0->field_1C;

    Gp_HaltPadScripts();
    SndEvt_EnqueueType7(0x54160001, 1);
    work->field_708 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141B60);

void func_actor_403600_80141BE0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320A0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_403600_80141C3C(Actor403600* arg0)
{
    s16 value;

    value = arg0->field_1C->field_730;
    if (value < 0) {
        return;
    }
    if (value < 2) {
        func_actor_403600_801400BC(arg0);
    }
}

void func_actor_403600_80141C7C(Actor403600* arg0, s32 arg1)
{
    GpEnemy*         enemy;
    Actor403600Work* work;

    enemy           = arg0->field_20;
    work            = arg0->field_1C;
    enemy->field_40 = *(u16*)&enemy->field_40 - arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->field_40 <= 0) {
        work->field_742 = 1;
    }
}

void func_actor_403600_80141CD4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_403600_801320EC;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141D30);

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141E78);

void func_actor_403600_80141F28(Actor403600* arg0)
{
    arg0->field_2C->field_8->sub = &Gfx_ViewCoord;
    Gp_EnemyTaskExit((Task*)arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403600/actor_403600_2", func_actor_403600_80141F58);
