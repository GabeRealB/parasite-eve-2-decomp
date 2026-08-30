#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D638);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", D_dryfield_breezeway_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D79C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D90C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017D940);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DA48);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DBA4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DBD8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DC3C);

void func_dryfield_breezeway_8017DCE4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (GameFlag_GetNibble(0x56) != 4) {
                    if (Gp_GetCapEventKey() == 0xB) {
                        GameFlag_SetNibble(0x56, 2);
                    }
                    if (GameFlag_GetNibble(0x56) == 5) {
                        GameFlag_SetNibble(0x56, 6);
                    }
                }
                Gp_MsgPlayerWeapon(1);
                Task_Kill(task);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DDB0);

void func_dryfield_breezeway_8017DE60(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DE68);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017DEC0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E010);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E114);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E2D4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E350);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E370);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E390);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E464);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", func_dryfield_breezeway_8017E81C);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", D_dryfield_breezeway_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway", jtbl_dryfield_breezeway_8017D608);
