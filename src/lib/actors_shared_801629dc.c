#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// The four `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks
/// from, and the message table it points the parent's `Task::msgTable` at:
/// ids 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `func_actor_350700_801636A8`, terminated by 0x7FFFFFFF.

s32 ActorsShared801629dc(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                               |= 0x80;
            ((Actor350700Work*)task->work)->field_4C5 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}
