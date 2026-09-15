#include "common.h"

#include "main/session.h"
#include "main/task.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. That
   also rules out `gameplay/D4.h`, whose prototype takes four arguments; the
   message table is named through that header's own struct tag instead. */
s32 Gp_DispatchMsg();

struct _GpMsgEntry;

extern struct _GpMsgEntry D_neo_ark_woodland_path_80181650[];
extern TaskDesc           D_neo_ark_woodland_path_80184A44[];
extern Task*              D_neo_ark_woodland_path_80181680;

s32 func_neo_ark_woodland_path_8017E8D4(void)
{
    return 0;
}

s32 func_neo_ark_woodland_path_8017E8DC(void)
{
    s32 ret;

    if (D_neo_ark_woodland_path_80181680 == NULL) {
        ret = -1;
    } else {
        ret = Gp_DispatchMsg(D_neo_ark_woodland_path_80181680);
    }
    return ret;
}

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

void func_neo_ark_woodland_path_8017E944(Task* arg0)
{
    arg0->field_24 = D_neo_ark_woodland_path_80181650;
    Game_SetPtrSlot(arg0, 7);
    D_neo_ark_woodland_path_80181680 = Task_SpawnFromTable(D_neo_ark_woodland_path_80184A44, 1, 0, 0);
    arg0->state                      = (s32)(arg0->state + 1);
}

void func_neo_ark_woodland_path_8017E9A8(void)
{
}
