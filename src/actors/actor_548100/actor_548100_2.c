#include "common.h"

#include "actors/actor_548100.h"

#include "main/gameflag.h"

extern u8 D_actor_548100_80135884;
extern u8 D_actor_548100_80135885;
extern u8 D_actor_548100_80135886;
extern u8 D_actor_548100_8013588A;
extern u8 D_actor_548100_8013588B;
extern u8 D_actor_548100_8013588C;

void func_actor_548100_80133684(Actor548100Edge* edge);
void func_actor_548100_80132A14(Task* arg0);
void func_actor_548100_80132EA0(Task* arg0);
void func_actor_548100_801330EC(void);

s32 func_actor_548100_80134778(Task* arg0, s16 arg1, s32 arg2)
{
    Actor548100Work* work = (Actor548100Work*)arg0->idMap;

    if ((arg2 == 0x120 || arg2 == 0x12C) && ((u16)work->step - 1) < 4U) {
        work->collectBitId = arg2;
        if (GameFlag_GetNibble(work->step + 0xBE) != 0 || GameFlag_GetNibble(0xC3) != 0) {
            return 2;
        }
        return 1;
    }
    work->collectBitId = 0;
    return 0;
}

void func_actor_548100_801347F8(Task* arg0)
{
    TaskFuncTable11 fns;

    fns = D_actor_548100_80131E6C;
    fns.funcs[arg0->state](arg0);
    func_actor_548100_801330EC();
    func_actor_548100_80132EA0(arg0);
    func_actor_548100_80132A14(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_801348A4);

void func_actor_548100_80134960(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135884 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135885 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135886 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_801349E0);

void func_actor_548100_80134A60(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_8013588A * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_8013588B * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_8013588C * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134AE0);

void func_actor_548100_80134BA8(void)
{
    Actor548100Edge* edge;

    for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
        func_actor_548100_80133684(edge);
    }
}

void func_actor_548100_80134BF0(void)
{
    Actor548100Edge* edge;

    if (GameFlag_GetNibble(0xBE) == 2) {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->field_2 == 2) {
                edge->state = 0;
            } else {
                edge->state = 1;
            }
        }
    } else {
        for (edge = D_actor_548100_801351D0; edge->nodeA != 0; edge++) {
            if (edge->field_2 == 1) {
                edge->state = 0;
            } else {
                edge->state = 1;
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_548100/actor_548100_2", func_actor_548100_80134CB8);
