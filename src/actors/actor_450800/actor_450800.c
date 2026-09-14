#include "common.h"
#include "main/task.h"

#include "actors/actor_450800.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"

extern s32  D_actor_450800_8013930C;
extern s32  D_actor_450800_801397A4;
extern s32  D_actor_450800_801398EC;
extern s32  D_actor_450800_8013A564;
extern s32  D_actor_450800_8013A684;
extern s32  D_actor_450800_8013A774;
extern s32  D_actor_450800_8013A984;
extern s32  D_actor_450800_8013AB7C;
extern s32  D_actor_450800_8013ACFC;
extern void func_80180038(s32);
extern void func_80182D14(s32, s32);

void func_actor_450800_80131E34(void)
{
    GameSession* session;
    s32          temp_v0;
    s32          n;

    if ((u8)session->field_4 == 4) {
        if (GameFlag_GetNibble(0xC7) == 1) {
            temp_v0                 = D_actor_450800_8013930C + 1;
            D_actor_450800_8013930C = temp_v0;
            if (temp_v0 >= 3) {
                D_actor_450800_8013930C = 3;
                func_800E8614((s32)&D_actor_450800_8013A774, 0);
            } else {
                func_800E8614((s32)&D_actor_450800_8013A684, 0);
            }
        } else {
            n = GameFlag_GetNibble(0xC8) + 1;
            if (n >= 4) {
                n = 3;
            }
            GameFlag_SetNibble(0xC8, n);
            if (n == 1) {
                if (GameFlag_GetNibble(0x83) == n) {
                    func_800E8614((s32)&D_actor_450800_8013A984, 0);
                } else {
                    func_800E8614((s32)&D_actor_450800_8013AB7C, 0);
                }
                func_800E3FAC(0xA2, 0x32);
            } else {
                func_800E8614((s32)&D_actor_450800_8013ACFC, 0);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80131F28);

void func_actor_450800_80131F70(u32 arg0)
{
    func_80182D14(arg0 >> 16, arg0 & 0xFFFF);
}

/// Two call sites, not one: `Gp_StartCapSlot` is written out in both arms of
/// the outer test. The tail-call cross-jump in `jump.c` merges them only from
/// the `jal` onward, because sched2 hoists the `a1`/`a2` setup away from the
/// call in the first arm before that pass runs - which is why the object sets
/// `$a1`/`$a2` twice and shares one `jal`.
///
/// The global is an `s32` (see `func_actor_450800_80131E34`, which increments
/// it whole), but this arm only wants its low half, which is the `lhu`.
void func_actor_450800_80131F98(s32 arg0)
{
    s16 var_a0;

    if (arg0 == 1) {
        var_a0 = (u16)D_actor_450800_8013930C + 2;
        Gp_StartCapSlot(var_a0, 0, 0);
    } else {
        if (GameFlag_GetNibble(0xC8) == 2) {
            var_a0 = 8;
        } else {
            var_a0 = 9;
        }
        Gp_StartCapSlot(var_a0, 0, 0);
    }
}

void func_actor_450800_80132000(void)
{
    func_800E8614((s32)&D_actor_450800_8013A564, 0);
}

void func_actor_450800_80132028(void)
{
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_450800_801397A4, 0);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_450800_801398EC, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132080);

void func_actor_450800_801320E8(s32 arg0)
{
    func_80180038(arg0 & 0xFF);
}

INCLUDE_RODATA("actors/nonmatchings/actor_450800/actor_450800", D_actor_450800_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_450800/actor_450800", D_actor_450800_80131E24);

void func_actor_450800_80132108(void)
{
    SVECTOR pos;

    pos = D_actor_450800_80131E24;
    Gp_SpawnEff(0x6003B, NULL, 0x200, &pos);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132160);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132448);

void func_actor_450800_80132160(void* enemy, Task* task);
void func_actor_450800_801327E4(void* enemy, Task* task);

void func_actor_450800_80132790(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132160, func_actor_450800_801327E4 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801327E4);

void func_actor_450800_80132868(Task* task)
{
    Actor450800Work* work = (Actor450800Work*)task->idMap;

    Gp_DestroyEnemy(task->spawnArg2, task);
    Task_Kill(work->field_4F0);
    Task_Kill(work->field_4F4);
    Task_Kill(work->field_4F8);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801328BC);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132958);
