#include "common.h"

#include "main/task.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern Task* D_neo_ark_woodland_path_80181680;

s32 func_neo_ark_woodland_path_8017E8D4(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_2", func_neo_ark_woodland_path_8017E8DC);

s32 func_neo_ark_woodland_path_8017E910(void)
{
    s32 ret;

    if (D_neo_ark_woodland_path_80181680 == NULL) {
        ret = -1;
    } else {
        ret = Gp_DispatchMsg(D_neo_ark_woodland_path_80181680);
    }
    return ret;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_2", func_neo_ark_woodland_path_8017E944);

void func_neo_ark_woodland_path_8017E9A8(void)
{
}
