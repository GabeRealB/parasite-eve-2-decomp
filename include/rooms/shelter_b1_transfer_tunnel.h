#ifndef ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
#define ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"

/// The room's three-state handler table, dispatched through a stack copy by
/// `func_shelter_b1_transfer_tunnel_8017D678`.
extern const TaskFuncTable3 D_shelter_b1_transfer_tunnel_8017D5C4;

/// The room's message table, parked in `Task::msgTable` by
/// `func_shelter_b1_transfer_tunnel_8017D62C`.
extern GpMsgEntry D_shelter_b1_transfer_tunnel_801828C0[];

void func_shelter_b1_transfer_tunnel_8017D62C(Task* task);
void func_shelter_b1_transfer_tunnel_8017D678(Task* task);
void func_shelter_b1_transfer_tunnel_8017D830(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b1_transfer_tunnel_8017DFAC(SVECTOR* arg0, s16 arg1);

#endif // ROOMS_SHELTER_B1_TRANSFER_TUNNEL_H
