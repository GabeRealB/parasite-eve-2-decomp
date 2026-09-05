#include "common.h"

#include "main/task.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */
s32 Gp_DispatchMsg();

extern Task* D_neo_ark_woodland_path_80181680;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_woodland_path/neo_ark_woodland_path_3", func_neo_ark_woodland_path_8017EA08);
