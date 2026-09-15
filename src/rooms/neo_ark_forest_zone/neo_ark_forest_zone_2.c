#include "common.h"

#include "main/task.h"

/* The room's own four state handlers, out of the leading rodata block in
 * `neo_ark_forest_zone.c`. A local initializer would emit the pool at this
 * function's .rodata instead of at D_neo_ark_forest_zone_8017D5D8. */
extern const TaskFuncTable4 D_neo_ark_forest_zone_8017D5D8;

/// Per-frame entry point: runs the handler its state selects. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_neo_ark_forest_zone_8017DBBC(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D5D8;
    sp.funcs[task->state](task);
}
