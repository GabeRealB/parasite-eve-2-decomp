#include "common.h"

#include "actors/actor_548100.h"

#include "main/gameflag.h"

extern u8 D_actor_548100_80135884;
extern u8 D_actor_548100_80135885;
extern u8 D_actor_548100_80135886;
extern u8 D_actor_548100_80135887;
extern u8 D_actor_548100_80135888;
extern u8 D_actor_548100_80135889;
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

s32 func_actor_548100_801348A4(Actor548100Hotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            if (hit == 0) {
                hit = table->id;
            }
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

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

void func_actor_548100_801349E0(s16 arg0, s8* arg1, s8* arg2, s8* arg3)
{
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;

    var_v0 = D_actor_548100_80135887 * arg0;
    if (var_v0 < 0) {
        var_v0 += 0x1F;
    }
    *arg1    = (s8)(var_v0 >> 5);
    var_v0_2 = D_actor_548100_80135888 * arg0;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x1F;
    }
    *arg2    = (s8)(var_v0_2 >> 5);
    var_v0_3 = D_actor_548100_80135889 * arg0;
    if (var_v0_3 < 0) {
        var_v0_3 += 0x1F;
    }
    *arg3 = (s8)(var_v0_3 >> 5);
}

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

void func_actor_548100_80134AE0(s32 id, u8 stop)
{
    u8* route;
    u8* head;
    u8  prev;
    u8  cur;
    u8  edge;
    u8  state;

    state = 2;
    if (stop != 0) {
        state = 3;
    }
    head  = D_actor_548100_80135B24[id];
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        cur = *route;
        if (cur != 0xFF) {
            edge                                = D_actor_548100_80135B5C[cur + prev * 100];
            D_actor_548100_801351D0[edge].state = state;
            prev                                = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == stop) {
            break;
        }
    }
}

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

s32 func_actor_548100_80134CB8(s32 nodeA, u8 nodeB)
{
    u8* route;
    u8* head;
    u8  prev;
    s32 dist;

    if (nodeA == 0) {
        return 1;
    }
    head  = D_actor_548100_80135B24[nodeA];
    dist  = 0;
    prev  = head[0];
    route = head + 1;
    while (*route != 0) {
        if (*route != 0xFF) {
            dist += D_actor_548100_801351D0[D_actor_548100_80135B5C[*route + prev * 100]].dist;
            prev  = *route;
        } else {
            route++;
            prev = *route;
        }
        if (*route++ == nodeB) {
            break;
        }
    }
    return dist;
}
