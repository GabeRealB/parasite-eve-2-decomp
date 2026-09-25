#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "main/tmd.h"

/// Setup state of the actor's child task (first entry of
/// `D_actor_443500_80131E24`): hides the child's model, then mirrors the
/// parent's (`spawnArg2`) hidden bit 0x80 and buffers-live bit 0x4 as the
/// per-frame state does. It draws the model at order-table offset -2, hangs
/// the child's root coordinate off the parent's part `spawnArg1`, shares the
/// parent's light and colour matrices, reparents the task under the parent and
/// steps to the next state.
void func_actor_443500_80132594(Task* task)
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
