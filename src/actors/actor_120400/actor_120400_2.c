#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor_120400.h"

#include "main/task.h"
#include "main/tmd.h"

/// Spawn state of a child task: its model starts hidden and then takes the
/// parent model's bits 0x80 and 0x4 as the tick does, allocating its buffers
/// while bit 0x4 is clear. The model is linked at ordering-table offset -2, its
/// root coordinate hangs off the parent's coordinate `spawnArg1`, it shares
/// the parent's light and colour matrices, and the task is reparented under
/// the parent before the state advances.
void func_actor_120400_801325A4(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400_2", D_actor_120400_80131E3C);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400_2", D_actor_120400_80131E4C);
