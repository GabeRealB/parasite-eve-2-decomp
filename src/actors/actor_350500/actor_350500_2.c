#include "common.h"

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

/// The walk steps, indexed by `Actor350500Work::field_4C2`: turn to face
/// `target`, start moving, approach until arrival, then turn to the placement
/// yaw.
const TaskFuncTable4 D_actor_350500_80161E30 = { {
    func_actor_350500_80162508,
    func_actor_350500_801625E4,
    func_actor_350500_80162038,
    func_actor_350500_8016272C,
} };

/// Republishes the work block's two matrices onto `TmdObject::lightMtx` /
/// `colorMtx`, so the actor draws with its own lighting.
void func_actor_350500_8016247C(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra;
    work          = (Actor350500Work*)arg0->work;
    ext->lightMtx = &work->light;
    ext->colorMtx = &work->color;
}

/// Idle tick handler, selected while `field_4C0` is clear.
void func_actor_350500_80162498(Task* arg0)
{
}

/// Walk tick handler: runs the step of `D_actor_350500_80161E30` that
/// `field_4C2` selects.
void func_actor_350500_801624A0(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor350500Work* work;

    work = (Actor350500Work*)arg0->work;
    sp   = D_actor_350500_80161E30;
    sp.funcs[(s16)work->field_4C2](arg0);
}
