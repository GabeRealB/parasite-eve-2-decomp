#include "common.h"

#include "main/task.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped (as in
   `src/rooms/neo_ark_woodland_path/neo_ark_woodland_path_2.c`). */
s32 Gp_DispatchMsg();

extern Task* D_mine_refuge_80182AD8;

s32 func_mine_refuge_8017FBB4(void)
{
    s32 ret;

    if (D_mine_refuge_80182AD8 == NULL) {
        ret = 0;
    } else {
        ret = Gp_DispatchMsg(D_mine_refuge_80182AD8);
    }
    return ret;
}
